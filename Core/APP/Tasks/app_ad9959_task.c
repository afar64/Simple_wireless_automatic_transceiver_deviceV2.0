#include "cmsis_os2.h"

#include "app_ad9959_task.h"
#include "app_dds_ctrl.h"
#include "app_si5351_drv.h"

extern osMessageQueueId_t DDSQueueHandle;

#define APP_LO_STARTUP_RETRY_MS    20U
#define APP_SI5351_MONITOR_MS      200U
#define APP_AD9959_FREQ_CAL_OFFSET_HZ 1896U

static volatile uint8_t s_baseband_ready = 0U;
static volatile uint32_t s_lo_freq_hz[APP_DDS_CHANNEL_COUNT] = {
  APP_AD9959_TASK_DEFAULT_FREQ_HZ,
  APP_AD9959_TASK_STARTUP_CH1_FREQ_HZ,
  APP_AD9959_TASK_DEFAULT_FREQ_HZ,
  APP_AD9959_TASK_DEFAULT_FREQ_HZ
};
static volatile uint16_t s_lo_amp_code[APP_DDS_CHANNEL_COUNT] = {
  0U,
  APP_AD9959_TASK_STARTUP_CH1_AMP_CODE,
  APP_AD9959_TASK_DEFAULT_AMP_CODE,
  APP_AD9959_TASK_DEFAULT_AMP_CODE
};
static volatile uint8_t s_lo_enable[APP_DDS_CHANNEL_COUNT] = {0U, 1U, 0U, 0U};
static volatile uint8_t s_lo_dirty_mask = 0x0FU;

static uint8_t App_LoTaskIsValidChannel(uint8_t ch)
{
  return (ch < APP_DDS_CHANNEL_COUNT) ? 1U : 0U;
}

static uint32_t App_LoTaskClampFrequencyHz(uint32_t freq_hz)
{
  if (freq_hz < APP_LO_FREQ_MIN_HZ)
  {
    return APP_LO_FREQ_MIN_HZ;
  }

  if (freq_hz > APP_LO_FREQ_MAX_HZ)
  {
    return APP_LO_FREQ_MAX_HZ;
  }

  return freq_hz;
}

static uint16_t App_LoTaskClampAmplitudeCode(uint16_t amp_code)
{
  if (amp_code > 1023U)
  {
    return 1023U;
  }

  return amp_code;
}

static uint32_t App_LoTaskApplyFrequencyCalibrationHz(uint32_t freq_hz)
{
  if (freq_hz > APP_AD9959_FREQ_CAL_OFFSET_HZ)
  {
    return freq_hz - APP_AD9959_FREQ_CAL_OFFSET_HZ;
  }

  return freq_hz;
}

static int App_LoTaskApplyAd9959ChannelState(uint8_t ch)
{
  int ret;
  uint32_t applied_freq_hz;

  if (App_LoTaskIsValidChannel(ch) == 0U)
  {
    return -1;
  }

  ret = AppDDS_SelectChannel(ch);
  if (ret != 0)
  {
    return ret;
  }

  applied_freq_hz = App_LoTaskApplyFrequencyCalibrationHz((uint32_t)s_lo_freq_hz[ch]);
  ret = AppDDS_SetFreq(applied_freq_hz);
  if (ret != 0)
  {
    return ret;
  }

  ret = AppDDS_SetAmp((s_lo_enable[ch] != 0U) ? s_lo_amp_code[ch] : 0U);
  if (ret != 0)
  {
    return ret;
  }

  return AppDDS_Apply();
}

static int App_LoTaskApplyAd9959BootDefaults(void)
{
  uint8_t ch;
  int ret;

  for (ch = 0U; ch < APP_DDS_CHANNEL_COUNT; ++ch)
  {
    ret = App_LoTaskApplyAd9959ChannelState(ch);
    if (ret != 0)
    {
      return ret;
    }
  }

  return 0;
}

static int App_LoTaskStartAd9959(void)
{
  int ret;

  ret = AppDDS_Init();
  if (ret != 0)
  {
    return ret;
  }

  return App_LoTaskApplyAd9959BootDefaults();
}

static int App_LoTaskStartSi5351(void)
{
  app_si5351_output_cfg_t cfg;
  app_si5351_result_t result;

  cfg.channel = APP_SI5351_TASK_DEFAULT_CHANNEL;
  cfg.freq_hz = (uint32_t)s_lo_freq_hz[APP_SI5351_TASK_DEFAULT_CHANNEL];
  cfg.pll = APP_SI5351_PLL_AUTO;
  cfg.drive = APP_SI5351_DRIVE_DEFAULT;
  cfg.enable = true;

  result = app_si5351_init_device();
  if (result != APP_SI5351_RESULT_OK)
  {
    return -(100 + (int)result);
  }

  result = app_si5351_apply_output_plan(&cfg, 1U);
  if (result != APP_SI5351_RESULT_OK)
  {
    return -(100 + (int)result);
  }

  result = app_si5351_enable_outputs(true);
  if (result != APP_SI5351_RESULT_OK)
  {
    return -(100 + (int)result);
  }

  return 0;
}

static int App_LoTaskApplySi5351Frequency(uint32_t freq_hz)
{
  app_si5351_output_cfg_t cfg;
  app_si5351_result_t result;

  cfg.channel = APP_SI5351_TASK_DEFAULT_CHANNEL;
  cfg.freq_hz = freq_hz;
  cfg.pll = APP_SI5351_PLL_AUTO;
  cfg.drive = APP_SI5351_DRIVE_DEFAULT;
  cfg.enable = true;

  result = app_si5351_apply_output_plan(&cfg, 1U);
  if (result != APP_SI5351_RESULT_OK)
  {
    return -(100 + (int)result);
  }

  result = app_si5351_enable_outputs(true);
  if (result != APP_SI5351_RESULT_OK)
  {
    return -(100 + (int)result);
  }

  return 0;
}

void App_LoTaskNotifyBasebandReady(void)
{
  s_baseband_ready = 1U;
}

uint32_t App_LoTaskGetFrequencyHz(void)
{
  return (uint32_t)s_lo_freq_hz[APP_AD9959_TASK_DEFAULT_CHANNEL];
}

int App_LoTaskSetFrequencyHz(uint32_t freq_hz)
{
  return App_LoTaskSetChannelFrequencyHz(APP_AD9959_TASK_DEFAULT_CHANNEL, freq_hz);
}

uint32_t App_LoTaskGetChannelFrequencyHz(uint8_t ch)
{
  if (App_LoTaskIsValidChannel(ch) == 0U)
  {
    return APP_AD9959_TASK_DEFAULT_FREQ_HZ;
  }

  return (uint32_t)s_lo_freq_hz[ch];
}

int App_LoTaskSetChannelFrequencyHz(uint8_t ch, uint32_t freq_hz)
{
  if (App_LoTaskIsValidChannel(ch) == 0U)
  {
    return -1;
  }

  s_lo_freq_hz[ch] = App_LoTaskClampFrequencyHz(freq_hz);
  s_lo_dirty_mask = (uint8_t)(s_lo_dirty_mask | (uint8_t)(1U << ch));
  return 0;
}

uint16_t App_LoTaskGetChannelAmplitudeCode(uint8_t ch)
{
  if (App_LoTaskIsValidChannel(ch) == 0U)
  {
    return APP_AD9959_TASK_DEFAULT_AMP_CODE;
  }

  return s_lo_amp_code[ch];
}

int App_LoTaskSetChannelAmplitudeCode(uint8_t ch, uint16_t amp_code)
{
  if (App_LoTaskIsValidChannel(ch) == 0U)
  {
    return -1;
  }

  s_lo_amp_code[ch] = App_LoTaskClampAmplitudeCode(amp_code);
  s_lo_dirty_mask = (uint8_t)(s_lo_dirty_mask | (uint8_t)(1U << ch));
  return 0;
}

uint8_t App_LoTaskGetChannelEnable(uint8_t ch)
{
  if (App_LoTaskIsValidChannel(ch) == 0U)
  {
    return 0U;
  }

  return s_lo_enable[ch];
}

int App_LoTaskSetChannelEnable(uint8_t ch, uint8_t enable)
{
  if (App_LoTaskIsValidChannel(ch) == 0U)
  {
    return -1;
  }

  s_lo_enable[ch] = (enable != 0U) ? 1U : 0U;
  s_lo_dirty_mask = (uint8_t)(s_lo_dirty_mask | (uint8_t)(1U << ch));
  return 0;
}

void StartDDSTask(void *argument)
{
#if APP_LO_SELECTED_SOURCE == APP_LO_SOURCE_AD9959
  AppDdsCmd cmd;
#endif
  uint8_t lo_started = 0U;

  (void)argument;

#if APP_LO_SELECTED_SOURCE == APP_LO_SOURCE_AD9959
  (void)AppDDS_BindRtosQueue(DDSQueueHandle);
#endif

  for (;;)
  {
    if (lo_started == 0U)
    {
      if (s_baseband_ready == 0U)
      {
        osDelay(5U);
        continue;
      }

#if APP_LO_SELECTED_SOURCE == APP_LO_SOURCE_AD9959
      if (App_LoTaskStartAd9959() != 0)
      {
        osDelay(APP_LO_STARTUP_RETRY_MS);
        continue;
      }
      lo_started = 1U;
      s_lo_dirty_mask = 0U;
#elif APP_LO_SELECTED_SOURCE == APP_LO_SOURCE_SI5351
      if (App_LoTaskStartSi5351() != 0)
      {
        osDelay(APP_LO_STARTUP_RETRY_MS);
        continue;
      }
      lo_started = 1U;
      s_lo_dirty_mask = 0U;
#else
#error "Unsupported APP_LO_SELECTED_SOURCE"
#endif
    }

    if (s_lo_dirty_mask != 0U)
    {
#if APP_LO_SELECTED_SOURCE == APP_LO_SOURCE_AD9959
      uint8_t ch;
      uint8_t pending = (uint8_t)s_lo_dirty_mask;

      for (ch = 0U; ch < APP_DDS_CHANNEL_COUNT; ++ch)
      {
        if ((pending & (uint8_t)(1U << ch)) == 0U)
        {
          continue;
        }

        if (App_LoTaskApplyAd9959ChannelState(ch) == 0)
        {
          s_lo_dirty_mask = (uint8_t)(s_lo_dirty_mask & (uint8_t)~(uint8_t)(1U << ch));
        }
      }
#else
      if (App_LoTaskApplySi5351Frequency((uint32_t)s_lo_freq_hz[APP_SI5351_TASK_DEFAULT_CHANNEL]) == 0)
      {
        s_lo_dirty_mask = 0U;
      }
#endif
    }

#if APP_LO_SELECTED_SOURCE == APP_LO_SOURCE_AD9959
    if (osMessageQueueGet(DDSQueueHandle, &cmd, NULL, 10U) == osOK)
    {
      (void)AppDDS_ExecuteCmd(&cmd);
    }
#else
    if (app_si5351_check_ref_status() != APP_SI5351_RESULT_OK)
    {
      (void)app_si5351_enable_outputs(false);
      lo_started = 0U;
      osDelay(APP_LO_STARTUP_RETRY_MS);
      continue;
    }

    osDelay(APP_SI5351_MONITOR_MS);
#endif
  }
}
