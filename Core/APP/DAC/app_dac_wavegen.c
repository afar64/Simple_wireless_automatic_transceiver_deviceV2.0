#include "app_dac_wavegen.h"

#include "app_ad9959_task.h"
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
#define APP_DAC_WAVE_PREFILL_TABLE_N 40000U
/* FM 预生成表至少要容纳一个调制周期；低于该频率时表长会超过 RAM_D2 预算。 */
#define APP_DAC_WAVE_FM_PREFILL_MIN_HZ \
  (APP_DAC_WAVE_SAMPLE_RATE_HZ / APP_DAC_WAVE_PREFILL_TABLE_N)
#define APP_DAC_WAVE_DAC_MAX_CODE    4095U
#define APP_DAC_WAVE_CACHE_LINE_SIZE 32U
#define APP_DAC_WAVE_PHASE_90_DEG    0x40000000UL
#define APP_DAC_WAVE_SINE_MID        2048
#define APP_DAC_WAVE_SINE_SCALE      2047

__attribute__((section(".dma_buffer"))) __attribute__((aligned(32)))
static uint16_t s_dac_ch1_buf[APP_DAC_WAVE_BUFFER_N];
__attribute__((section(".dma_buffer"))) __attribute__((aligned(32)))
static uint16_t s_dac_ch2_buf[APP_DAC_WAVE_BUFFER_N];
/* 预生成表也放在 RAM_D2，保证 DMA 相关数据避开 H743 上 DMA 不可达的 DTCM。 */
__attribute__((section(".dma_buffer"))) __attribute__((aligned(32)))
static uint16_t s_prefill_ch1_table[APP_DAC_WAVE_PREFILL_TABLE_N];
__attribute__((section(".dma_buffer"))) __attribute__((aligned(32)))
static uint16_t s_prefill_ch2_table[APP_DAC_WAVE_PREFILL_TABLE_N];

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
    .mode = APP_DAC_WAVE_DEFAULT_MODE,
    .vpp_mv = APP_DAC_WAVE_DEFAULT_VPP_MV,
    .mod_freq_hz = APP_DAC_WAVE_DEFAULT_MOD_FREQ_HZ,
    .symbol_rate_bps = APP_DAC_WAVE_DEFAULT_SYMBOL_RATE_BPS,
    .am_depth_percent = APP_DAC_WAVE_DEFAULT_AM_DEPTH_PERCENT,
    .fm_deviation_hz = APP_DAC_WAVE_DEFAULT_FM_DEVIATION_HZ,
    .fsk_shift_hz = APP_DAC_WAVE_DEFAULT_FSK_SHIFT_HZ,
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
static uint32_t s_prefill_table_len = 0U;
static uint32_t s_prefill_read_index = 0U;
static uint16_t s_dc_offset_code = 0U;
static uint16_t s_peak_code = 0U;
static uint16_t s_lfsr = 0x5A5AU;
static uint8_t s_current_bit = 1U;
/* 默认输出规律 01，保留 LFSR 入口便于后续切回伪随机测试码流。 */
static uint8_t s_use_lfsr_pattern = 0U;

static void App_DacWavegenSanitizeConfig(AppDacWavegenConfig *config);
static uint32_t App_DacWavegenCalcPhaseStep(uint32_t freq_hz);
static uint16_t App_DacWavegenMvToCode(uint16_t mv);
static int32_t App_DacWavegenSinSigned(uint32_t phase);
static uint16_t App_DacWavegenSignedToCode(int32_t signed_code);
static void App_DacWavegenNextIqSample(uint16_t *i_code, uint16_t *q_code);
static void App_DacWavegenFillBlock(uint32_t offset, uint32_t count);
static void App_DacWavegenFillBlockFromPrefill(uint32_t offset, uint32_t count);
static void App_DacWavegenCleanCacheRange(const void *addr, uint32_t size);
static void App_DacWavegenApplyDerivedConfig(const AppDacWavegenConfig *config);
static void App_DacWavegenBuildPrefillTable(const AppDacWavegenConfig *config);
static void App_DacWavegenBuildFmPrefillTable(const AppDacWavegenConfig *config);
static void App_DacWavegenBuildFskPrefillTable(const AppDacWavegenConfig *config);
static void App_DacWavegenResetPhaseAndSymbols(void);
static uint8_t App_DacWavegenUsePrefillTable(void);
static uint8_t App_DacWavegenNextBit(void);
static uint8_t App_DacWavegenGetSymbolBit(void);

void App_DacWavegenSetDefaultConfig(AppDacWavegenConfig *config)
{
  if (config == NULL)
  {
    return;
  }

  config->mode = APP_DAC_WAVE_DEFAULT_MODE;
  config->vpp_mv = APP_DAC_WAVE_DEFAULT_VPP_MV;
  config->mod_freq_hz = APP_DAC_WAVE_DEFAULT_MOD_FREQ_HZ;
  config->symbol_rate_bps = APP_DAC_WAVE_DEFAULT_SYMBOL_RATE_BPS;
  config->am_depth_percent = APP_DAC_WAVE_DEFAULT_AM_DEPTH_PERCENT;
  config->fm_deviation_hz = APP_DAC_WAVE_DEFAULT_FM_DEVIATION_HZ;
  config->fsk_shift_hz = APP_DAC_WAVE_DEFAULT_FSK_SHIFT_HZ;
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

  /* 复杂模式在任务上下文预生成，避免 DMA 回调里做 64 位乘除和三角函数查表组合。 */
  App_DacWavegenBuildPrefillTable(&local);

  if (was_running != 0U)
  {
    App_DacWavegenResetPhaseAndSymbols();
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

  /* 先建表再填 DMA 首帧，保证启动 TIM6 前两个 DAC 缓冲区已有完整数据。 */
  App_DacWavegenBuildPrefillTable(&s_status.config);
  App_DacWavegenResetPhaseAndSymbols();

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
  App_LoTaskNotifyBasebandReady();
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

  /* DMA 正在输出后半区时重填前半区；FM/2FSK 此处只从预生成表搬运。 */
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

  /* DMA 正在回到前半区时重填后半区，保持双路 I/Q 缓冲同步推进。 */
  App_DacWavegenFillBlock(APP_DAC_WAVE_HALF_BUFFER_N, APP_DAC_WAVE_HALF_BUFFER_N);
  App_DacWavegenCleanCacheRange(&s_dac_ch1_buf[APP_DAC_WAVE_HALF_BUFFER_N],
                                APP_DAC_WAVE_HALF_BUFFER_N * sizeof(uint16_t));
  App_DacWavegenCleanCacheRange(&s_dac_ch2_buf[APP_DAC_WAVE_HALF_BUFFER_N],
                                APP_DAC_WAVE_HALF_BUFFER_N * sizeof(uint16_t));
  s_status.full_irq_count++;
}

static void App_DacWavegenSanitizeConfig(AppDacWavegenConfig *config)
{
  if (config->mode > APP_DAC_WAVE_MODE_CW)
  {
    config->mode = APP_DAC_WAVE_MODE_AM;
  }
  if (config->vpp_mv < APP_DAC_WAVE_IQ_VPP_MIN_MV)
  {
    config->vpp_mv = APP_DAC_WAVE_IQ_VPP_MIN_MV;
  }
  if (config->vpp_mv > APP_DAC_WAVE_IQ_VPP_MAX_MV)
  {
    config->vpp_mv = APP_DAC_WAVE_IQ_VPP_MAX_MV;
  }
  if (config->mod_freq_hz < APP_DAC_WAVE_MOD_FREQ_MIN_HZ)
  {
    config->mod_freq_hz = APP_DAC_WAVE_MOD_FREQ_MIN_HZ;
  }
  if (config->mod_freq_hz > APP_DAC_WAVE_MOD_FREQ_MAX_HZ)
  {
    config->mod_freq_hz = APP_DAC_WAVE_MOD_FREQ_MAX_HZ;
  }
  if ((config->mode == APP_DAC_WAVE_MODE_FM) &&
      (config->mod_freq_hz < APP_DAC_WAVE_FM_PREFILL_MIN_HZ))
  {
    /* 低频 FM 需要更长表；当前阶段优先守住 RAM_D2 和中断实时性。 */
    config->mod_freq_hz = APP_DAC_WAVE_FM_PREFILL_MIN_HZ;
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
  /* FSK 参数表示两频点间隔，因此内部用一半得到 -shift/2 与 +shift/2。 */
  s_fsk_shift_step = App_DacWavegenCalcPhaseStep(config->fsk_shift_hz / 2U);
  s_dc_offset_code = App_DacWavegenMvToCode(APP_DAC_WAVE_IQ_DC_OFFSET_MV);
  s_peak_code = App_DacWavegenMvToCode(config->vpp_mv / 2U);
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
  /* 所有调制分量最终都围绕固定直流偏置输出，DAC 端不产生负电压。 */
  int32_t code = (int32_t)s_dc_offset_code + signed_code;

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
  uint8_t bit;
  uint32_t active_step;

  switch (s_status.config.mode)
  {
    case APP_DAC_WAVE_MODE_AM:
      sin_mod = App_DacWavegenSinSigned(s_mod_phase_acc);
      s_mod_phase_acc += s_mod_phase_step;
      /* 当前 AM 输出为围绕 400 mV 的基带调幅分量，不额外抬高平均值。 */
      i_signed = (int32_t)s_peak_code +
                 (((int32_t)s_peak_code *
                   (int32_t)s_status.config.am_depth_percent *
                   sin_mod) /
                  (100 * APP_DAC_WAVE_SINE_SCALE));
      q_signed = 0;
      break;

    case APP_DAC_WAVE_MODE_FM:
      /* 实时路径只作备用；正常 FM 会走预生成表，避免回调中重复重计算。 */
      sin_mod = App_DacWavegenSinSigned(s_mod_phase_acc);
      s_mod_phase_acc += s_mod_phase_step;
      s_carrier_phase_acc += s_carrier_phase_step +
                             (uint32_t)(((int64_t)(int32_t)s_fm_deviation_step * sin_mod) /
                                        APP_DAC_WAVE_SINE_SCALE);
      sin_carrier = App_DacWavegenSinSigned(s_carrier_phase_acc);
      cos_carrier = App_DacWavegenSinSigned(s_carrier_phase_acc + APP_DAC_WAVE_PHASE_90_DEG);
      i_signed = ((int32_t)s_peak_code * cos_carrier) / APP_DAC_WAVE_SINE_SCALE;
      q_signed = ((int32_t)s_peak_code * sin_carrier) / APP_DAC_WAVE_SINE_SCALE;
      break;

    case APP_DAC_WAVE_MODE_2ASK:
      bit = App_DacWavegenGetSymbolBit();
      i_signed = (bit != 0U) ? (int32_t)s_peak_code : 0;
      q_signed = 0;
      break;

    case APP_DAC_WAVE_MODE_2PSK:
      bit = App_DacWavegenGetSymbolBit();
      i_signed = (bit != 0U) ? (int32_t)s_peak_code : -(int32_t)s_peak_code;
      q_signed = 0;
      break;

    case APP_DAC_WAVE_MODE_2FSK:
    case APP_DAC_WAVE_MODE_CW:
    default:
      bit = App_DacWavegenGetSymbolBit();
      /* 连续相位 FSK：0/1 分别对应 -shift/2 与 +shift/2，不再使用 0/+shift。 */
      active_step = (bit != 0U) ? s_fsk_shift_step : (uint32_t)(0U - s_fsk_shift_step);
      s_carrier_phase_acc += active_step;
      sin_carrier = App_DacWavegenSinSigned(s_carrier_phase_acc);
      cos_carrier = App_DacWavegenSinSigned(s_carrier_phase_acc + APP_DAC_WAVE_PHASE_90_DEG);
      i_signed = ((int32_t)s_peak_code * cos_carrier) / APP_DAC_WAVE_SINE_SCALE;
      q_signed = ((int32_t)s_peak_code * sin_carrier) / APP_DAC_WAVE_SINE_SCALE;
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

  if (App_DacWavegenUsePrefillTable() != 0U)
  {
    /* FM/2FSK 的重计算已前移到建表阶段，这里只顺序复制表数据。 */
    App_DacWavegenFillBlockFromPrefill(offset, count);
    return;
  }

  for (i = 0U; i < count; i++)
  {
    App_DacWavegenNextIqSample(&i_code, &q_code);
    s_dac_ch1_buf[offset + i] = i_code;
    s_dac_ch2_buf[offset + i] = q_code;
  }
}

static void App_DacWavegenFillBlockFromPrefill(uint32_t offset, uint32_t count)
{
  uint32_t i;

  for (i = 0U; i < count; i++)
  {
    s_dac_ch1_buf[offset + i] = s_prefill_ch1_table[s_prefill_read_index];
    s_dac_ch2_buf[offset + i] = s_prefill_ch2_table[s_prefill_read_index];
    s_prefill_read_index++;
    if (s_prefill_read_index >= s_prefill_table_len)
    {
      s_prefill_read_index = 0U;
    }
  }
}

static void App_DacWavegenCleanCacheRange(const void *addr, uint32_t size)
{
  uintptr_t start = (uintptr_t)addr;
  uintptr_t aligned_start = start & ~(uintptr_t)(APP_DAC_WAVE_CACHE_LINE_SIZE - 1U);
  uintptr_t aligned_end = (start + (uintptr_t)size + (APP_DAC_WAVE_CACHE_LINE_SIZE - 1U)) &
                          ~(uintptr_t)(APP_DAC_WAVE_CACHE_LINE_SIZE - 1U);

  /* CPU 写入 DMA 缓冲后必须 Clean DCache，否则 DAC DMA 可能读到旧样本。 */
  SCB_CleanDCache_by_Addr((uint32_t *)aligned_start, (int32_t)(aligned_end - aligned_start));
}

static void App_DacWavegenBuildPrefillTable(const AppDacWavegenConfig *config)
{
  s_prefill_table_len = 0U;
  s_prefill_read_index = 0U;

  if (config->mode == APP_DAC_WAVE_MODE_FM)
  {
    /* FM 生成一个调制周期表，循环播放时相位和包络首尾连续。 */
    App_DacWavegenBuildFmPrefillTable(config);
  }
  else if (config->mode == APP_DAC_WAVE_MODE_2FSK)
  {
    /* 2FSK 生成两个符号周期表，对应当前规律 01 码流。 */
    App_DacWavegenBuildFskPrefillTable(config);
  }
}

static void App_DacWavegenBuildFmPrefillTable(const AppDacWavegenConfig *config)
{
  uint32_t i;
  uint32_t table_len;
  uint32_t carrier_phase = 0U;
  uint32_t mod_phase = 0U;
  int32_t sin_mod;
  int32_t sin_carrier;
  int32_t cos_carrier;
  int32_t i_signed;
  int32_t q_signed;

  table_len = APP_DAC_WAVE_SAMPLE_RATE_HZ / config->mod_freq_hz;
  if (table_len == 0U)
  {
    table_len = 1U;
  }
  if (table_len > APP_DAC_WAVE_PREFILL_TABLE_N)
  {
    table_len = APP_DAC_WAVE_PREFILL_TABLE_N;
  }

  for (i = 0U; i < table_len; i++)
  {
    /* 建表阶段允许复杂计算；DMA 回调只复制 s_prefill_*_table。 */
    sin_mod = App_DacWavegenSinSigned(mod_phase);
    mod_phase += s_mod_phase_step;
    carrier_phase += s_carrier_phase_step +
                     (uint32_t)(((int64_t)(int32_t)s_fm_deviation_step * sin_mod) /
                                APP_DAC_WAVE_SINE_SCALE);
    sin_carrier = App_DacWavegenSinSigned(carrier_phase);
    cos_carrier = App_DacWavegenSinSigned(carrier_phase + APP_DAC_WAVE_PHASE_90_DEG);
    i_signed = ((int32_t)s_peak_code * cos_carrier) / APP_DAC_WAVE_SINE_SCALE;
    q_signed = ((int32_t)s_peak_code * sin_carrier) / APP_DAC_WAVE_SINE_SCALE;
    s_prefill_ch1_table[i] = App_DacWavegenSignedToCode(i_signed);
    s_prefill_ch2_table[i] = App_DacWavegenSignedToCode(q_signed);
  }

  s_prefill_table_len = table_len;
}

static void App_DacWavegenBuildFskPrefillTable(const AppDacWavegenConfig *config)
{
  uint32_t i;
  uint32_t table_len;
  uint32_t carrier_phase = 0U;
  uint32_t active_step;
  uint32_t symbol_index;
  uint8_t bit;
  int32_t sin_carrier;
  int32_t cos_carrier;
  int32_t i_signed;
  int32_t q_signed;

  (void)config;

  table_len = s_samples_per_symbol * 2U;
  if (table_len == 0U)
  {
    table_len = 2U;
  }
  if (table_len > APP_DAC_WAVE_PREFILL_TABLE_N)
  {
    table_len = APP_DAC_WAVE_PREFILL_TABLE_N;
  }

  for (i = 0U; i < table_len; i++)
  {
    symbol_index = i / s_samples_per_symbol;
    bit = (uint8_t)(symbol_index & 1U);
    /* 表内相位连续累加，只在频率步进上切换，避免符号边界相位跳变。 */
    active_step = (bit != 0U) ? s_fsk_shift_step : (uint32_t)(0U - s_fsk_shift_step);
    carrier_phase += active_step;
    sin_carrier = App_DacWavegenSinSigned(carrier_phase);
    cos_carrier = App_DacWavegenSinSigned(carrier_phase + APP_DAC_WAVE_PHASE_90_DEG);
    i_signed = ((int32_t)s_peak_code * cos_carrier) / APP_DAC_WAVE_SINE_SCALE;
    q_signed = ((int32_t)s_peak_code * sin_carrier) / APP_DAC_WAVE_SINE_SCALE;
    s_prefill_ch1_table[i] = App_DacWavegenSignedToCode(i_signed);
    s_prefill_ch2_table[i] = App_DacWavegenSignedToCode(q_signed);
  }

  s_prefill_table_len = table_len;
}

static void App_DacWavegenResetPhaseAndSymbols(void)
{
  s_carrier_phase_acc = 0U;
  s_mod_phase_acc = 0U;
  s_symbol_countdown = 0U;
  s_lfsr = 0x5A5AU;
  s_current_bit = 1U;
  s_prefill_read_index = 0U;
}

static uint8_t App_DacWavegenUsePrefillTable(void)
{
  if (((s_status.config.mode == APP_DAC_WAVE_MODE_FM) ||
       (s_status.config.mode == APP_DAC_WAVE_MODE_2FSK)) &&
      (s_prefill_table_len > 0U))
  {
    return 1U;
  }

  return 0U;
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
    if (s_use_lfsr_pattern != 0U)
    {
      s_current_bit = App_DacWavegenNextBit();
    }
    else
    {
      /* 当前测试优先使用规律 01，便于在示波器和频谱仪上稳定观察码元边界。 */
      s_current_bit ^= 1U;
    }
    s_symbol_countdown = s_samples_per_symbol;
  }

  s_symbol_countdown--;
  return s_current_bit;
}
