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
#define APP_DAC_WAVE_CACHE_LINE_SIZE 32U

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
    .freq_hz = 1000U,
    .low_mv = 0U,
    .high_mv = 3300U,
  },
  .running = 0U,
  .last_error = 0,
  .half_irq_count = 0U,
  .full_irq_count = 0U,
};

static uint32_t s_phase_acc = 0U;
static uint32_t s_phase_step = 0U;
static uint16_t s_low_code = 0U;
static uint16_t s_high_code = APP_DAC_WAVE_DAC_MAX_CODE;

static void App_DacWavegenSanitizeConfig(AppDacWavegenConfig *config);
static uint32_t App_DacWavegenCalcPhaseStep(uint32_t freq_hz);
static uint16_t App_DacWavegenMvToCode(uint16_t mv);
static uint16_t App_DacWavegenNextSample(void);
static void App_DacWavegenFillBlock(uint32_t offset, uint32_t count);
static void App_DacWavegenCleanCacheRange(const void *addr, uint32_t size);
static void App_DacWavegenApplyDerivedConfig(const AppDacWavegenConfig *config);

void App_DacWavegenSetDefaultConfig(AppDacWavegenConfig *config)
{
  if (config == NULL)
  {
    return;
  }

  config->freq_hz = 1000U;
  config->low_mv = 0U;
  config->high_mv = 3300U;
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
    s_phase_acc = 0U;
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

  (void)HAL_TIM_Base_Stop(&htim6);  // 确保定时器停止，避免在配置过程中触发中断。
  (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
  (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
  (void)HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_2);

  __disable_irq();  // 先禁用中断，避免在配置过程中触发中断导致状态不一致。
  App_DacWavegenSanitizeConfig(&s_status.config); // 确保当前配置参数合法。
  App_DacWavegenApplyDerivedConfig(&s_status.config); // 根据当前配置计算派生参数。
  s_phase_acc = 0U;
  App_DacWavegenSanitizeConfig(&s_status.config);
  App_DacWavegenApplyDerivedConfig(&s_status.config);
  s_phase_acc = 0U;
  s_status.half_irq_count = 0U;
  s_status.full_irq_count = 0U;
  __enable_irq(); // 配置完成后再启用中断。

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

static void App_DacWavegenSanitizeConfig(AppDacWavegenConfig *config) // 确保配置参数在合理范围内，并修正不合理的参数组合。
{
  uint16_t swap;

  if (config->freq_hz < APP_DAC_WAVE_FREQ_MIN_HZ)
  {
    config->freq_hz = APP_DAC_WAVE_FREQ_MIN_HZ;
  }
  if (config->freq_hz > APP_DAC_WAVE_FREQ_MAX_HZ)
  {
    config->freq_hz = APP_DAC_WAVE_FREQ_MAX_HZ;
  }
  if (config->low_mv > APP_DAC_WAVE_MV_MAX)
  {
    config->low_mv = APP_DAC_WAVE_MV_MAX;
  }
  if (config->high_mv > APP_DAC_WAVE_MV_MAX)
  {
    config->high_mv = APP_DAC_WAVE_MV_MAX;
  }
  if (config->low_mv > config->high_mv)
  {
    swap = config->low_mv;
    config->low_mv = config->high_mv;
    config->high_mv = swap;
  }
}

static void App_DacWavegenApplyDerivedConfig(const AppDacWavegenConfig *config)
{
  s_phase_step = App_DacWavegenCalcPhaseStep(config->freq_hz);
  s_low_code = App_DacWavegenMvToCode(config->low_mv);
  s_high_code = App_DacWavegenMvToCode(config->high_mv);
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

static uint16_t App_DacWavegenNextSample(void)
{
  uint32_t index;
  uint32_t sine;
  uint32_t span;
  uint32_t code;

  index = s_phase_acc >> (32U - APP_DAC_WAVE_TABLE_BITS);
  s_phase_acc += s_phase_step;

  sine = s_sine_table[index];
  span = (uint32_t)(s_high_code - s_low_code);
  code = (uint32_t)s_low_code + ((span * sine + 2047U) / APP_DAC_WAVE_DAC_MAX_CODE);

  if (code > APP_DAC_WAVE_DAC_MAX_CODE)
  {
    code = APP_DAC_WAVE_DAC_MAX_CODE;
  }

  return (uint16_t)code;
}

static void App_DacWavegenFillBlock(uint32_t offset, uint32_t count)
{
  uint32_t i;
  uint16_t sample;

  for (i = 0U; i < count; i++)
  {
    sample = App_DacWavegenNextSample();
    s_dac_ch1_buf[offset + i] = sample;
    s_dac_ch2_buf[offset + i] = sample;
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
