#ifndef APP_TX_CONTROL_H
#define APP_TX_CONTROL_H

#include <stdint.h>

#include "app_ad9959_task.h"
#include "app_dac_wavegen.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_TX_CONTROL_FREQ_MIN_HZ   110000000UL
#define APP_TX_CONTROL_FREQ_MAX_HZ   130000000UL
#define APP_TX_CONTROL_FREQ_STEP_HZ     100000UL
#define APP_TX_CONTROL_PRESET_DEFAULT_FREQ_HZ 120000000UL
#define APP_TX_CONTROL_SWEEP_PERIOD_MS      2000UL
#define APP_TX_CONTROL_SWEEP_PERIOD_MIN_MS  1000UL
#define APP_TX_CONTROL_SWEEP_PERIOD_MAX_MS  5000UL
#define APP_TX_CONTROL_SWEEP_PERIOD_STEP_MS 1000UL
#define APP_TX_CONTROL_SWEEP_START_DEFAULT_HZ APP_TX_CONTROL_FREQ_MIN_HZ
#define APP_TX_CONTROL_SWEEP_STOP_DEFAULT_HZ  APP_TX_CONTROL_FREQ_MAX_HZ

typedef struct
{
  uint8_t tx_on;
  uint8_t sweep_on;
  AppDacWavegenMode mode;
  uint32_t freq_hz;
  uint16_t amplitude_mv;
} AppTxControlBasicState;

typedef struct
{
  AppTxControlBasicState basic;
  AppDacWavegenStatus dac_status;
} AppTxControlSnapshot;

void App_TxControl_Init(void);
void App_TxControl_GetSnapshot(AppTxControlSnapshot *snapshot);
int App_TxControl_SetMode(AppDacWavegenMode mode);
int App_TxControl_SetFrequencyHz(uint32_t freq_hz);
int App_TxControl_SetAmplitudeMv(uint16_t amplitude_mv);
int App_TxControl_SetConfig(const AppDacWavegenConfig *config);
int App_TxControl_GetConfig(AppDacWavegenConfig *config, uint32_t *freq_hz);
int App_TxControl_Apply(void);
int App_TxControl_Start(void);
int App_TxControl_Stop(void);
uint32_t App_TxControl_ClampFrequencyHz(uint32_t freq_hz);
uint32_t App_TxControl_GetPresetFrequencyHz(void);
int App_TxControl_SavePresetFrequencyHz(uint32_t freq_hz);
int App_TxControl_RecallPresetFrequencyHz(uint32_t *freq_hz);
int App_TxControl_SetSweepEnabled(uint8_t enable);
uint32_t App_TxControl_GetSweepPeriodMs(void);
int App_TxControl_SetSweepPeriodMs(uint32_t period_ms);
uint32_t App_TxControl_GetSweepStartHz(void);
uint32_t App_TxControl_GetSweepStopHz(void);
int App_TxControl_SetSweepRangeHz(uint32_t start_hz, uint32_t stop_hz);
void App_TxControl_Service(uint32_t now_ms);
int App_TxControl_SetDebugModeEnabled(uint8_t enable);
uint8_t App_TxControl_GetDebugModeEnabled(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_TX_CONTROL_H */
