#include "app_dac_wavegen.h"

#include "dac.h"
#include "main.h"
#include "tim.h"

#include "cmsis_os2.h"

#include <stddef.h>
#include <stdint.h>

#define APP_DAC_WAVE_TABLE_BITS      8U
#define APP_DAC_WAVE_TABLE_SIZE      (1U << APP_DAC_WAVE_TABLE_BITS)
#define APP_DAC_WAVE_BUFFER_N        2048U
#define APP_DAC_WAVE_HALF_BUFFER_N   (APP_DAC_WAVE_BUFFER_N / 2U)
#define APP_DAC_WAVE_DAC_MAX_CODE    4095U
#define APP_DAC_WAVE_DAC_MID_CODE    2048U
#define APP_DAC_WAVE_CACHE_LINE_SIZE 32U
#define APP_DAC_WAVE_PHASE_90_DEG    0x40000000UL
#define APP_DAC_WAVE_SINE_MID        2048
#define APP_DAC_WAVE_SINE_SCALE      2047

__attribute__((section(".dma_buffer"))) __attribute__((aligned(32)))
static uint16_t s_dac_ch1_buf[APP_DAC_WAVE_BUFFER_N];
__attribute__((section(".dma_buffer"))) __attribute__((aligned(32)))
static uint16_t s_dac_ch2_buf[APP_DAC_WAVE_BUFFER_N];

static const uint16_t s_sine_table[APP_DAC_WAVE_TABLE_SIZE] = {
  2048, 2098, 2148, 2198, 2248, 2298, 2348, 2398,
  2447, 2496, 2545, 2594, 2642, 2690, 2737, 2784,
  2831, 2877, 2923, 2968, 3013, 3057, 3100, 3143,
  3185, 3226, 3267, 3307, 3346, 3385, 3423, 3459,
  3495, 3530, 3565, 3598, 3630, 3662, 3692, 3722,
  3750, 3777, 3804, 3829, 3853, 3876, 3898, 3919,
  3939, 3958, 3975, 3992, 4007, 4021, 4034, 4045,
  4056, 4065, 4073, 4080, 4085, 4089, 4093, 4094,
  4095, 4094, 4093, 4089, 4085, 4080, 4073, 4065,
  4056, 4045, 4034, 4021, 4007, 3992, 3975, 3958,
  3939, 3919, 3898, 3876, 3853, 3829, 3804, 3777,
  3750, 3722, 3692, 3662, 3630, 3598, 3565, 3530,
  3495, 3459, 3423, 3385, 3346, 3307, 3267, 3226,
  3185, 3143, 3100, 3057, 3013, 2968, 2923, 2877,
  2831, 2784, 2737, 2690, 2642, 2594, 2545, 2496,
  2447, 2398, 2348, 2298, 2248, 2198, 2148, 2098,
  2048, 1997, 1947, 1897, 1847, 1797, 1747, 1697,
  1648, 1599, 1550, 1501, 1453, 1405, 1358, 1311,
  1264, 1218, 1172, 1127, 1082, 1038,  995,  952,
   910,  869,  828,  788,  749,  710,  672,  636,
   600,  565,  530,  497,  465,  433,  403,  373,
   345,  318,  291,  266,  242,  219,  197,  176,
   156,  137,  120,  103,   88,   74,   61,   50,
    39,   30,   22,   15,   10,    6,    2,    1,
     0,    1,    2,    6,   10,   15,   22,   30,
    39,   50,   61,   74,   88,  103,  120,  137,
   156,  176,  197,  219,  242,  266,  291,  318,
   345,  373,  403,  433,  465,  497,  530,  565,
   600,  636,  672,  710,  749,  788,  828,  869,
   910,  952,  995, 1038, 1082, 1127, 1172, 1218,
  1264, 1311, 1358, 1405, 1453, 1501, 1550, 1599,
  1648, 1697, 1747, 1797, 1847, 1897, 1947, 1997
};

static AppDacWavegenStatus s_status = {
  .config = {
    .mode = APP_DAC_WAVE_MODE_AM,
    .amplitude_mv = 1000U,
    .mod_freq_hz = 10000U,
    .symbol_rate_bps = 10000U,
    .am_depth_percent = 50U,
    .fm_deviation_hz = 75000U,
    .fsk_shift_hz = 20000U,
  },
  .running = 0U,
  .last_error = 0,
  .half_irq_count = 0U,
  .full_irq_count = 0U,
};

static uint32_t s_carrier_phase_acc = 0U;
static uint32_t s_mod_phase_acc = 0U;
static uint32_t s_carrier_phase_step = 0U;
static uint32_t s_mod_phase_step = 0U;
static uint32_t s_fm_deviation_step = 0U;
static uint32_t s_fsk_shift_step = 0U;
static uint32_t s_samples_per_symbol = 1U;
static uint32_t s_symbol_countdown = 0U;
static uint16_t s_amplitude_code = 0U;
static uint16_t s_lfsr = 0x5A5AU;
static uint8_t s_current_bit = 1U;

static void App_DacWavegenSanitizeConfig(AppDacWavegenConfig *config);
static uint32_t App_DacWavegenCalcPhaseStep(uint32_t freq_hz);
static uint16_t App_DacWavegenMvToCode(uint16_t mv);
static int32_t App_DacWavegenSinSigned(uint32_t phase);
static uint16_t App_DacWavegenSignedToCode(int32_t signed_code);
static void App_DacWavegenNextIqSample(uint16_t *i_code, uint16_t *q_code);
static void App_DacWavegenFillBlock(uint32_t offset, uint32_t count);
static void App_DacWavegenCleanCacheRange(const void *addr, uint32_t size);
static void App_DacWavegenApplyDerivedConfig(const AppDacWavegenConfig *config);
static void App_DacWavegenResetPhaseAndSymbols(void);
static uint8_t App_DacWavegenNextBit(void);
static uint8_t App_DacWavegenGetSymbolBit(void);

void App_DacWavegenSetDefaultConfig(AppDacWavegenConfig *config)
{
  if (config == NULL)
  {
    return;
  }

  config->mode = APP_DAC_WAVE_MODE_AM;
  config->amplitude_mv = 1000U;
  config->mod_freq_hz = 10000U;
  config->symbol_rate_bps = 10000U;
  config->am_depth_percent = 50U;
  config->fm_deviation_hz = 75000U;
  config->fsk_shift_hz = 20000U;
}

int App_DacWavegenSetConfig(const AppDacWavegenConfig *config)
{
  AppDacWavegenConfig local;
  uint8_t was_running;

  if (config == NULL)
  {
    return -1;
  }

  local = *config;
  App_DacWavegenSanitizeConfig(&local);

  was_running = s_status.running;
  if (was_running != 0U)
  {
    (void)HAL_TIM_Base_Stop(&htim6);
  }

  __disable_irq();
  s_status.config = local;
  App_DacWavegenApplyDerivedConfig(&local);
  if (was_running != 0U)
  {
    App_DacWavegenResetPhaseAndSymbols();
  }
  __enable_irq();

  if (was_running != 0U)
  {
    App_DacWavegenFillBlock(0U, APP_DAC_WAVE_BUFFER_N);
    App_DacWavegenCleanCacheRange(s_dac_ch1_buf, sizeof(s_dac_ch1_buf));
    App_DacWavegenCleanCacheRange(s_dac_ch2_buf, sizeof(s_dac_ch2_buf));
    (void)HAL_TIM_Base_Start(&htim6);
  }

  return 0;
}

void App_DacWavegenGetStatus(AppDacWavegenStatus *status)
{
  if (status == NULL)
  {
    return;
  }

  __disable_irq();
  *status = s_status;
  __enable_irq();
}

int App_DacWavegenStart(void)
{
  HAL_StatusTypeDef hal_status;

  (void)HAL_TIM_Base_Stop(&htim6);
  (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
  (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_2);

  __disable_irq();
  App_DacWavegenSanitizeConfig(&s_status.config);
  App_DacWavegenApplyDerivedConfig(&s_status.config);
  App_DacWavegenResetPhaseAndSymbols();
  s_status.half_irq_count = 0U;
  s_status.full_irq_count = 0U;
  __enable_irq();

  App_DacWavegenFillBlock(0U, APP_DAC_WAVE_BUFFER_N);
  App_DacWavegenCleanCacheRange(s_dac_ch1_buf, sizeof(s_dac_ch1_buf));
  App_DacWavegenCleanCacheRange(s_dac_ch2_buf, sizeof(s_dac_ch2_buf));

  hal_status = HAL_DAC_Start_DMA(&hdac1,
                                 DAC_CHANNEL_1,
                                 (uint32_t *)s_dac_ch1_buf,
                                 APP_DAC_WAVE_BUFFER_N,
                                 DAC_ALIGN_12B_R);
  if (hal_status != HAL_OK)
  {
    s_status.last_error = -2;
    return -2;
  }

  hal_status = HAL_DAC_Start_DMA(&hdac1,
                                 DAC_CHANNEL_2,
                                 (uint32_t *)s_dac_ch2_buf,
                                 APP_DAC_WAVE_BUFFER_N,
                                 DAC_ALIGN_12B_R);
  if (hal_status != HAL_OK)
  {
    s_status.last_error = -3;
    return -3;
  }

  hal_status = HAL_TIM_Base_Start(&htim6);
  if (hal_status != HAL_OK)
  {
    s_status.last_error = -4;
    return -4;
  }

  s_status.running = 1U;
  s_status.last_error = 0;
  return 0;
}

void App_DacWavegenStop(void)
{
  (void)HAL_TIM_Base_Stop(&htim6);
  (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
  (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_2);

  __disable_irq();
  s_status.running = 0U;
  __enable_irq();
}

void App_DacWavegenTaskRun(void *argument)
{
  (void)argument;

  (void)App_DacWavegenStart();

  for (;;)
  {
    osDelay(100U);
  }
}

void StartAdcTask(void *argument)
{
  App_DacWavegenTaskRun(argument);
}

void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
  if (hdac->Instance != DAC1)
  {
    return;
  }

  App_DacWavegenFillBlock(0U, APP_DAC_WAVE_HALF_BUFFER_N);
  App_DacWavegenCleanCacheRange(s_dac_ch1_buf, APP_DAC_WAVE_HALF_BUFFER_N * sizeof(uint16_t));
  App_DacWavegenCleanCacheRange(s_dac_ch2_buf, APP_DAC_WAVE_HALF_BUFFER_N * sizeof(uint16_t));
  s_status.half_irq_count++;
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
  if (hdac->Instance != DAC1)
  {
    return;
  }

  App_DacWavegenFillBlock(APP_DAC_WAVE_HALF_BUFFER_N, APP_DAC_WAVE_HALF_BUFFER_N);
  App_DacWavegenCleanCacheRange(&s_dac_ch1_buf[APP_DAC_WAVE_HALF_BUFFER_N],
                                APP_DAC_WAVE_HALF_BUFFER_N * sizeof(uint16_t));
  App_DacWavegenCleanCacheRange(&s_dac_ch2_buf[APP_DAC_WAVE_HALF_BUFFER_N],
                                APP_DAC_WAVE_HALF_BUFFER_N * sizeof(uint16_t));
  s_status.full_irq_count++;
}

static void App_DacWavegenSanitizeConfig(AppDacWavegenConfig *config)
{
  if (config->mode > APP_DAC_WAVE_MODE_2FSK)
  {
    config->mode = APP_DAC_WAVE_MODE_AM;
  }
  if (config->amplitude_mv < APP_DAC_WAVE_IQ_AMPLITUDE_MIN_MV)
  {
    config->amplitude_mv = APP_DAC_WAVE_IQ_AMPLITUDE_MIN_MV;
  }
  if (config->amplitude_mv > APP_DAC_WAVE_IQ_AMPLITUDE_MAX_MV)
  {
    config->amplitude_mv = APP_DAC_WAVE_IQ_AMPLITUDE_MAX_MV;
  }
  if (config->mod_freq_hz < APP_DAC_WAVE_MOD_FREQ_MIN_HZ)
  {
    config->mod_freq_hz = APP_DAC_WAVE_MOD_FREQ_MIN_HZ;
  }
  if (config->mod_freq_hz > APP_DAC_WAVE_MOD_FREQ_MAX_HZ)
  {
    config->mod_freq_hz = APP_DAC_WAVE_MOD_FREQ_MAX_HZ;
  }
  if (config->symbol_rate_bps < APP_DAC_WAVE_SYMBOL_RATE_MIN_BPS)
  {
    config->symbol_rate_bps = APP_DAC_WAVE_SYMBOL_RATE_MIN_BPS;
  }
  if (config->symbol_rate_bps > APP_DAC_WAVE_SYMBOL_RATE_MAX_BPS)
  {
    config->symbol_rate_bps = APP_DAC_WAVE_SYMBOL_RATE_MAX_BPS;
  }
  if (config->am_depth_percent > APP_DAC_WAVE_AM_DEPTH_MAX_PERCENT)
  {
    config->am_depth_percent = APP_DAC_WAVE_AM_DEPTH_MAX_PERCENT;
  }
  if (config->fm_deviation_hz > APP_DAC_WAVE_FM_DEVIATION_MAX_HZ)
  {
    config->fm_deviation_hz = APP_DAC_WAVE_FM_DEVIATION_MAX_HZ;
  }
  if (config->fsk_shift_hz > APP_DAC_WAVE_FSK_SHIFT_MAX_HZ)
  {
    config->fsk_shift_hz = APP_DAC_WAVE_FSK_SHIFT_MAX_HZ;
  }
}

static void App_DacWavegenApplyDerivedConfig(const AppDacWavegenConfig *config)
{
  s_carrier_phase_step = 0U;
  s_mod_phase_step = App_DacWavegenCalcPhaseStep(config->mod_freq_hz);
  s_fm_deviation_step = App_DacWavegenCalcPhaseStep(config->fm_deviation_hz);
  s_fsk_shift_step = App_DacWavegenCalcPhaseStep(config->fsk_shift_hz);
  s_amplitude_code = App_DacWavegenMvToCode(config->amplitude_mv);
  s_samples_per_symbol = APP_DAC_WAVE_SAMPLE_RATE_HZ / config->symbol_rate_bps;
  if (s_samples_per_symbol == 0U)
  {
    s_samples_per_symbol = 1U;
  }
}

static uint32_t App_DacWavegenCalcPhaseStep(uint32_t freq_hz)
{
  return (uint32_t)(((uint64_t)freq_hz << 32) / APP_DAC_WAVE_SAMPLE_RATE_HZ);
}

static uint16_t App_DacWavegenMvToCode(uint16_t mv)
{
  return (uint16_t)((((uint32_t)mv * APP_DAC_WAVE_DAC_MAX_CODE) + (APP_DAC_WAVE_DAC_REF_MV / 2U)) /
                    APP_DAC_WAVE_DAC_REF_MV);
}

static int32_t App_DacWavegenSinSigned(uint32_t phase)
{
  uint32_t index = phase >> (32U - APP_DAC_WAVE_TABLE_BITS);
  return (int32_t)s_sine_table[index] - APP_DAC_WAVE_SINE_MID;
}

static uint16_t App_DacWavegenSignedToCode(int32_t signed_code)
{
  int32_t code = (int32_t)APP_DAC_WAVE_DAC_MID_CODE + signed_code;

  if (code < 0)
  {
    code = 0;
  }
  if (code > (int32_t)APP_DAC_WAVE_DAC_MAX_CODE)
  {
    code = (int32_t)APP_DAC_WAVE_DAC_MAX_CODE;
  }

  return (uint16_t)code;
}

static void App_DacWavegenNextIqSample(uint16_t *i_code, uint16_t *q_code)
{
  int32_t i_signed = 0;
  int32_t q_signed = 0;
  int32_t sin_mod;
  int32_t sin_carrier;
  int32_t cos_carrier;
  int32_t envelope_code;
  uint8_t bit;
  uint32_t active_step;

  switch (s_status.config.mode)
  {
    case APP_DAC_WAVE_MODE_AM:
      sin_mod = App_DacWavegenSinSigned(s_mod_phase_acc);
      s_mod_phase_acc += s_mod_phase_step;
      envelope_code = (int32_t)s_amplitude_code;
      envelope_code += ((int32_t)s_amplitude_code *
                        (int32_t)s_status.config.am_depth_percent *
                        sin_mod) /
                       (100 * APP_DAC_WAVE_SINE_SCALE);
      i_signed = envelope_code;
      q_signed = 0;
      break;

    case APP_DAC_WAVE_MODE_FM:
      sin_mod = App_DacWavegenSinSigned(s_mod_phase_acc);
      s_mod_phase_acc += s_mod_phase_step;
      s_carrier_phase_acc += s_carrier_phase_step +
                             (uint32_t)(((int64_t)(int32_t)s_fm_deviation_step * sin_mod) /
                                        APP_DAC_WAVE_SINE_SCALE);
      sin_carrier = App_DacWavegenSinSigned(s_carrier_phase_acc);
      cos_carrier = App_DacWavegenSinSigned(s_carrier_phase_acc + APP_DAC_WAVE_PHASE_90_DEG);
      i_signed = ((int32_t)s_amplitude_code * cos_carrier) / APP_DAC_WAVE_SINE_SCALE;
      q_signed = ((int32_t)s_amplitude_code * sin_carrier) / APP_DAC_WAVE_SINE_SCALE;
      break;

    case APP_DAC_WAVE_MODE_2ASK:
      bit = App_DacWavegenGetSymbolBit();
      i_signed = (bit != 0U) ? (int32_t)s_amplitude_code : 0;
      q_signed = 0;
      break;

    case APP_DAC_WAVE_MODE_2PSK:
      bit = App_DacWavegenGetSymbolBit();
      i_signed = (bit != 0U) ? (int32_t)s_amplitude_code : -(int32_t)s_amplitude_code;
      q_signed = 0;
      break;

    case APP_DAC_WAVE_MODE_2FSK:
    default:
      bit = App_DacWavegenGetSymbolBit();
      active_step = (bit != 0U) ? s_fsk_shift_step : 0U;
      s_carrier_phase_acc += active_step;
      sin_carrier = App_DacWavegenSinSigned(s_carrier_phase_acc);
      cos_carrier = App_DacWavegenSinSigned(s_carrier_phase_acc + APP_DAC_WAVE_PHASE_90_DEG);
      i_signed = ((int32_t)s_amplitude_code * cos_carrier) / APP_DAC_WAVE_SINE_SCALE;
      q_signed = ((int32_t)s_amplitude_code * sin_carrier) / APP_DAC_WAVE_SINE_SCALE;
      break;
  }

  *i_code = App_DacWavegenSignedToCode(i_signed);
  *q_code = App_DacWavegenSignedToCode(q_signed);
}

static void App_DacWavegenFillBlock(uint32_t offset, uint32_t count)
{
  uint32_t i;
  uint16_t i_code;
  uint16_t q_code;

  for (i = 0U; i < count; i++)
  {
    App_DacWavegenNextIqSample(&i_code, &q_code);
    s_dac_ch1_buf[offset + i] = i_code;
    s_dac_ch2_buf[offset + i] = q_code;
  }
}

static void App_DacWavegenCleanCacheRange(const void *addr, uint32_t size)
{
  uintptr_t start = (uintptr_t)addr;
  uintptr_t aligned_start = start & ~(uintptr_t)(APP_DAC_WAVE_CACHE_LINE_SIZE - 1U);
  uintptr_t aligned_end = (start + (uintptr_t)size + (APP_DAC_WAVE_CACHE_LINE_SIZE - 1U)) &
                          ~(uintptr_t)(APP_DAC_WAVE_CACHE_LINE_SIZE - 1U);

  SCB_CleanDCache_by_Addr((uint32_t *)aligned_start, (int32_t)(aligned_end - aligned_start));
}

static void App_DacWavegenResetPhaseAndSymbols(void)
{
  s_carrier_phase_acc = 0U;
  s_mod_phase_acc = 0U;
  s_symbol_countdown = 0U;
  s_lfsr = 0x5A5AU;
  s_current_bit = 1U;
}

static uint8_t App_DacWavegenNextBit(void)
{
  uint16_t new_bit = (uint16_t)(((s_lfsr >> 0) ^ (s_lfsr >> 2) ^ (s_lfsr >> 3) ^ (s_lfsr >> 5)) & 1U);
  s_lfsr = (uint16_t)((s_lfsr >> 1) | (new_bit << 15));
  return (uint8_t)(s_lfsr & 1U);
}

static uint8_t App_DacWavegenGetSymbolBit(void)
{
  if (s_symbol_countdown == 0U)
  {
    s_current_bit = App_DacWavegenNextBit();
    s_symbol_countdown = s_samples_per_symbol;
  }

  s_symbol_countdown--;
  return s_current_bit;
}
