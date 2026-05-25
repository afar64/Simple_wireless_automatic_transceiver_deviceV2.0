#include "app_tx_control.h"

#include "app_ad9959_task.h"
#include "main.h"

#include <string.h>

static AppDacWavegenConfig s_desired_config;
static uint32_t s_desired_lo_freq_hz = APP_AD9959_TASK_DEFAULT_FREQ_HZ;
static uint32_t s_preset_lo_freq_hz = APP_TX_CONTROL_PRESET_DEFAULT_FREQ_HZ;
static uint32_t s_sweep_period_ms = APP_TX_CONTROL_SWEEP_PERIOD_MS;
static uint32_t s_sweep_start_hz = APP_TX_CONTROL_SWEEP_START_DEFAULT_HZ;
static uint32_t s_sweep_stop_hz = APP_TX_CONTROL_SWEEP_STOP_DEFAULT_HZ;
static uint8_t s_desired_valid = 0U;
static uint8_t s_sweep_enabled = 0U;
static uint8_t s_debug_mode_enabled = 0U;
static uint8_t s_calibration_output_enabled = 0U;
static int32_t s_lo_error_hz = 0;
static uint32_t s_sweep_last_tick_ms = 0U;
static uint32_t s_sweep_start_tick_ms = 0U;

static uint8_t App_TxControlIsCwMode(AppDacWavegenMode mode)
{
  return (mode == APP_DAC_WAVE_MODE_CW) ? 1U : 0U;
}

static void App_TxControlUpdateRelayForMode(AppDacWavegenMode mode)
{
  GPIO_PinState state = GPIO_PIN_SET;

  if (App_TxControlIsCwMode(mode) != 0U)
  {
    state = GPIO_PIN_RESET;
  }

  HAL_GPIO_WritePin(REL1_GPIO_Port, REL1_Pin, state);
}

static uint8_t App_TxControlGetLoChannelForMode(AppDacWavegenMode mode)
{
  return (App_TxControlIsCwMode(mode) != 0U) ? APP_AD9959_TASK_CW_CHANNEL : APP_AD9959_TASK_MOD_CHANNEL;
}

static uint16_t App_TxControlMapCwAmplitudeCode(uint16_t amplitude_mv)
{
  if (amplitude_mv > 1023U)
  {
    return 1023U;
  }

  return amplitude_mv;
}

static uint32_t App_TxControlClampSweepPeriodMs(uint32_t period_ms)
{
  if (period_ms < APP_TX_CONTROL_SWEEP_PERIOD_MIN_MS)
  {
    return APP_TX_CONTROL_SWEEP_PERIOD_MIN_MS;
  }

  if (period_ms > APP_TX_CONTROL_SWEEP_PERIOD_MAX_MS)
  {
    return APP_TX_CONTROL_SWEEP_PERIOD_MAX_MS;
  }

  return period_ms;
}

static uint32_t App_TxControlApplyFrequencyErrorHz(uint32_t freq_hz)
{
  int64_t adjusted = (int64_t)freq_hz + (int64_t)s_lo_error_hz;

  if (adjusted < (int64_t)APP_TX_CONTROL_FREQ_MIN_HZ)
  {
    return APP_TX_CONTROL_FREQ_MIN_HZ;
  }

  if (adjusted > (int64_t)APP_TX_CONTROL_FREQ_MAX_HZ)
  {
    return APP_TX_CONTROL_FREQ_MAX_HZ;
  }

  return (uint32_t)adjusted;
}

uint32_t App_TxControl_ClampFrequencyHz(uint32_t freq_hz)
{
  if (freq_hz < APP_TX_CONTROL_FREQ_MIN_HZ)
  {
    return APP_TX_CONTROL_FREQ_MIN_HZ;
  }

  if (freq_hz > APP_TX_CONTROL_FREQ_MAX_HZ)
  {
    return APP_TX_CONTROL_FREQ_MAX_HZ;
  }

  return freq_hz;
}

static void App_TxControlLoadDesiredFromHardware(void)
{
  AppDacWavegenStatus status;
  uint8_t ch;

  App_DacWavegenGetStatus(&status);
  s_desired_config = status.config;
  ch = App_TxControlGetLoChannelForMode(s_desired_config.mode);
  s_desired_lo_freq_hz = App_TxControl_ClampFrequencyHz(App_LoTaskGetChannelFrequencyHz(ch));
  s_desired_valid = 1U;
}

static void App_TxControlEnsureDesiredLoaded(void)
{
  if (s_desired_valid == 0U)
  {
    App_TxControlLoadDesiredFromHardware();
  }
}

static void App_TxControlApplyLoRouting(uint8_t enable_target_channel)
{
  uint8_t target_ch = App_TxControlGetLoChannelForMode(s_desired_config.mode);
  uint8_t other_ch = (target_ch == APP_AD9959_TASK_CW_CHANNEL) ? APP_AD9959_TASK_MOD_CHANNEL : APP_AD9959_TASK_CW_CHANNEL;
  uint16_t target_amp_code = APP_AD9959_TASK_DEFAULT_AMP_CODE;

  if (App_TxControlIsCwMode(s_desired_config.mode) != 0U)
  {
    target_amp_code = App_TxControlMapCwAmplitudeCode(s_desired_config.vpp_mv);
  }

  (void)App_LoTaskSetChannelFrequencyHz(target_ch, App_TxControlApplyFrequencyErrorHz(s_desired_lo_freq_hz));
  (void)App_LoTaskSetChannelAmplitudeCode(target_ch, target_amp_code);
  (void)App_LoTaskSetChannelEnable(target_ch, enable_target_channel);
  (void)App_LoTaskSetChannelEnable(other_ch, 0U);
}

static void App_TxControlApplyCurrentFrequencyOnly(void)
{
  uint8_t target_ch = App_TxControlGetLoChannelForMode(s_desired_config.mode);

  (void)App_LoTaskSetChannelFrequencyHz(target_ch, App_TxControlApplyFrequencyErrorHz(s_desired_lo_freq_hz));
}

void App_TxControl_Init(void)
{
  App_TxControlLoadDesiredFromHardware();
  s_preset_lo_freq_hz = App_TxControl_ClampFrequencyHz(s_preset_lo_freq_hz);
  s_sweep_period_ms = App_TxControlClampSweepPeriodMs(s_sweep_period_ms);
  s_sweep_start_hz = App_TxControl_ClampFrequencyHz(s_sweep_start_hz);
  s_sweep_stop_hz = App_TxControl_ClampFrequencyHz(s_sweep_stop_hz);
  App_TxControlUpdateRelayForMode(s_desired_config.mode);
}

void App_TxControl_GetSnapshot(AppTxControlSnapshot *snapshot)
{
  if (snapshot == NULL)
  {
    return;
  }

  memset(snapshot, 0, sizeof(*snapshot));
  App_DacWavegenGetStatus(&snapshot->dac_status);
  snapshot->basic.mode = snapshot->dac_status.config.mode;
  snapshot->basic.freq_hz = App_TxControl_ClampFrequencyHz(s_desired_lo_freq_hz);
  snapshot->basic.amplitude_mv = snapshot->dac_status.config.vpp_mv;
  snapshot->basic.sweep_on = s_sweep_enabled;
  if (App_TxControlIsCwMode(snapshot->basic.mode) != 0U)
  {
    snapshot->basic.tx_on = App_LoTaskGetChannelEnable(APP_AD9959_TASK_CW_CHANNEL);
  }
  else
  {
    snapshot->basic.tx_on = snapshot->dac_status.running;
  }
}

int App_TxControl_SetMode(AppDacWavegenMode mode)
{
  App_TxControlEnsureDesiredLoaded();
  s_desired_config.mode = mode;
  s_sweep_enabled = 0U;
  App_TxControlUpdateRelayForMode(mode);
  return 0;
}

int App_TxControl_SetFrequencyHz(uint32_t freq_hz)
{
  App_TxControlEnsureDesiredLoaded();
  s_desired_lo_freq_hz = App_TxControl_ClampFrequencyHz(freq_hz);
  return 0;
}

int App_TxControl_SetAmplitudeMv(uint16_t amplitude_mv)
{
  App_TxControlEnsureDesiredLoaded();
  s_desired_config.vpp_mv = amplitude_mv;
  return 0;
}

int App_TxControl_SetConfig(const AppDacWavegenConfig *config)
{
  if (config == NULL)
  {
    return -1;
  }

  App_TxControlEnsureDesiredLoaded();
  s_desired_config = *config;
  return 0;
}

int App_TxControl_GetConfig(AppDacWavegenConfig *config, uint32_t *freq_hz)
{
  App_TxControlEnsureDesiredLoaded();

  if (config != NULL)
  {
    *config = s_desired_config;
  }

  if (freq_hz != NULL)
  {
    *freq_hz = App_TxControl_ClampFrequencyHz(s_desired_lo_freq_hz);
  }

  return 0;
}

int App_TxControl_Apply(void)
{
  AppDacWavegenStatus status;

  App_TxControlEnsureDesiredLoaded();
  App_DacWavegenGetStatus(&status);
  App_TxControlUpdateRelayForMode(s_desired_config.mode);

  if (App_TxControlIsCwMode(s_desired_config.mode) != 0U)
  {
    App_DacWavegenStop();
    (void)App_DacWavegenSetConfig(&s_desired_config);
    App_TxControlApplyLoRouting(App_LoTaskGetChannelEnable(APP_AD9959_TASK_CW_CHANNEL));
  }
  else
  {
    (void)App_DacWavegenSetConfig(&s_desired_config);
    App_TxControlApplyLoRouting(status.running);
  }

  return 0;
}

int App_TxControl_Start(void)
{
  App_TxControlEnsureDesiredLoaded();
  App_TxControlUpdateRelayForMode(s_desired_config.mode);

  if (App_TxControlIsCwMode(s_desired_config.mode) != 0U)
  {
    App_DacWavegenStop();
    (void)App_DacWavegenSetConfig(&s_desired_config);
    App_TxControlApplyLoRouting(1U);
  }
  else
  {
    App_TxControlApplyLoRouting(1U);
    (void)App_DacWavegenSetConfig(&s_desired_config);
    (void)App_DacWavegenStart();
  }

  return 0;
}

int App_TxControl_Stop(void)
{
  s_sweep_enabled = 0U;
  s_calibration_output_enabled = 0U;
  App_DacWavegenStop();
  (void)App_LoTaskSetChannelEnable(APP_AD9959_TASK_MOD_CHANNEL, 0U);
  (void)App_LoTaskSetChannelEnable(APP_AD9959_TASK_CW_CHANNEL, 0U);
  App_TxControlUpdateRelayForMode(s_desired_config.mode);
  return 0;
}

uint32_t App_TxControl_GetPresetFrequencyHz(void)
{
  return App_TxControl_ClampFrequencyHz(s_preset_lo_freq_hz);
}

int App_TxControl_SavePresetFrequencyHz(uint32_t freq_hz)
{
  s_preset_lo_freq_hz = App_TxControl_ClampFrequencyHz(freq_hz);
  return 0;
}

int App_TxControl_RecallPresetFrequencyHz(uint32_t *freq_hz)
{
  App_TxControlEnsureDesiredLoaded();
  s_sweep_enabled = 0U;
  s_desired_lo_freq_hz = App_TxControl_ClampFrequencyHz(s_preset_lo_freq_hz);

  if (freq_hz != NULL)
  {
    *freq_hz = s_desired_lo_freq_hz;
  }

  return 0;
}

int App_TxControl_SetSweepEnabled(uint8_t enable)
{
  App_TxControlEnsureDesiredLoaded();

  if (enable != 0U)
  {
    s_sweep_enabled = 1U;
    s_sweep_last_tick_ms = 0U;
    s_sweep_start_tick_ms = 0U;
    s_desired_lo_freq_hz = s_sweep_start_hz;
    App_TxControlApplyCurrentFrequencyOnly();
  }
  else
  {
    s_sweep_enabled = 0U;
  }

  return 0;
}

uint32_t App_TxControl_GetSweepPeriodMs(void)
{
  return App_TxControlClampSweepPeriodMs(s_sweep_period_ms);
}

int App_TxControl_SetSweepPeriodMs(uint32_t period_ms)
{
  s_sweep_period_ms = App_TxControlClampSweepPeriodMs(period_ms);
  return 0;
}

uint32_t App_TxControl_GetSweepStartHz(void)
{
  return s_sweep_start_hz;
}

uint32_t App_TxControl_GetSweepStopHz(void)
{
  return s_sweep_stop_hz;
}

int App_TxControl_SetSweepRangeHz(uint32_t start_hz, uint32_t stop_hz)
{
  uint32_t start_aligned = App_TxControl_ClampFrequencyHz(start_hz);
  uint32_t stop_aligned = App_TxControl_ClampFrequencyHz(stop_hz);

  if (start_aligned > stop_aligned)
  {
    uint32_t tmp = start_aligned;
    start_aligned = stop_aligned;
    stop_aligned = tmp;
  }

  s_sweep_start_hz = start_aligned;
  s_sweep_stop_hz = stop_aligned;

  if (s_sweep_enabled != 0U)
  {
    s_desired_lo_freq_hz = s_sweep_start_hz;
    s_sweep_start_tick_ms = 0U;
    s_sweep_last_tick_ms = 0U;
    App_TxControlApplyCurrentFrequencyOnly();
  }

  return 0;
}

void App_TxControl_Service(uint32_t now_ms)
{
  uint32_t period_ms;
  uint32_t elapsed_ms;
  uint32_t next_freq_hz;
  uint32_t sweep_span_hz;

  if (s_sweep_enabled == 0U)
  {
    return;
  }

  if (s_sweep_start_tick_ms == 0U)
  {
    s_sweep_start_tick_ms = now_ms;
    s_sweep_last_tick_ms = 0U;
  }

  if ((s_sweep_last_tick_ms != 0U) && ((now_ms - s_sweep_last_tick_ms) < 5U))
  {
    return;
  }

  s_sweep_last_tick_ms = now_ms;
  period_ms = App_TxControlClampSweepPeriodMs(s_sweep_period_ms);
  elapsed_ms = now_ms - s_sweep_start_tick_ms;

  if (period_ms == 0U)
  {
    period_ms = 1U;
  }

  elapsed_ms %= period_ms;
  sweep_span_hz = s_sweep_stop_hz - s_sweep_start_hz;
  next_freq_hz = s_sweep_start_hz;
  if (sweep_span_hz != 0U)
  {
    next_freq_hz += (uint32_t)(((uint64_t)elapsed_ms * (uint64_t)sweep_span_hz) / (uint64_t)period_ms);
  }

  if (next_freq_hz != s_desired_lo_freq_hz)
  {
    s_desired_lo_freq_hz = next_freq_hz;
    App_TxControlApplyCurrentFrequencyOnly();
  }
}

int App_TxControl_SetDebugModeEnabled(uint8_t enable)
{
  s_debug_mode_enabled = (enable != 0U) ? 1U : 0U;
  return 0;
}

uint8_t App_TxControl_GetDebugModeEnabled(void)
{
  return s_debug_mode_enabled;
}

int App_TxControl_StartCalibrationOutput(void)
{
  uint32_t calibration_freq_hz = App_TxControlApplyFrequencyErrorHz(120000000UL);

  App_TxControlEnsureDesiredLoaded();
  s_sweep_enabled = 0U;
  s_calibration_output_enabled = 1U;
  App_DacWavegenStop();
  App_TxControlUpdateRelayForMode(APP_DAC_WAVE_MODE_CW);
  (void)App_LoTaskSetChannelEnable(APP_AD9959_TASK_MOD_CHANNEL, 0U);
  (void)App_LoTaskSetChannelFrequencyHz(APP_AD9959_TASK_CW_CHANNEL, calibration_freq_hz);
  (void)App_LoTaskSetChannelAmplitudeCode(APP_AD9959_TASK_CW_CHANNEL, 1023U);
  (void)App_LoTaskSetChannelEnable(APP_AD9959_TASK_CW_CHANNEL, 1U);
  return 0;
}

int App_TxControl_StopCalibrationOutput(void)
{
  s_calibration_output_enabled = 0U;
  App_DacWavegenStop();
  (void)App_LoTaskSetChannelEnable(APP_AD9959_TASK_MOD_CHANNEL, 0U);
  (void)App_LoTaskSetChannelEnable(APP_AD9959_TASK_CW_CHANNEL, 0U);
  return 0;
}

uint8_t App_TxControl_GetCalibrationOutputEnabled(void)
{
  return s_calibration_output_enabled;
}

int App_TxControl_SetLoErrorHz(int32_t error_hz)
{
  s_lo_error_hz = error_hz;
  return 0;
}

int32_t App_TxControl_GetLoErrorHz(void)
{
  return s_lo_error_hz;
}
