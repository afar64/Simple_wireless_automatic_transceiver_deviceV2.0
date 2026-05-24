#ifndef APP_DAC_WAVEGEN_H
#define APP_DAC_WAVEGEN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_DAC_WAVE_SAMPLE_RATE_HZ          2000000UL
#define APP_DAC_WAVE_DAC_REF_MV             3531U
/* IQ 输出先按板级实测参考电压换算 DAC 码值，再叠加 400 mV 直流偏置。 */
#define APP_DAC_WAVE_IQ_DC_OFFSET_MV        1000U
#define APP_DAC_WAVE_IQ_VPP_MIN_MV          0U
#define APP_DAC_WAVE_IQ_VPP_MAX_MV          1000U
#define APP_DAC_WAVE_MOD_FREQ_MIN_HZ        1UL
#define APP_DAC_WAVE_MOD_FREQ_MAX_HZ        100000UL
#define APP_DAC_WAVE_SYMBOL_RATE_MIN_BPS    100UL
#define APP_DAC_WAVE_SYMBOL_RATE_MAX_BPS    50000UL
#define APP_DAC_WAVE_AM_DEPTH_MIN_PERCENT   0U
#define APP_DAC_WAVE_AM_DEPTH_MAX_PERCENT   100U
#define APP_DAC_WAVE_FM_DEVIATION_MIN_HZ    0UL
#define APP_DAC_WAVE_FM_DEVIATION_MAX_HZ    100000UL
#define APP_DAC_WAVE_FSK_SHIFT_MIN_HZ       0UL
#define APP_DAC_WAVE_FSK_SHIFT_MAX_HZ       100000UL

typedef enum
{
  APP_DAC_WAVE_MODE_AM = 0,
  APP_DAC_WAVE_MODE_FM,
  APP_DAC_WAVE_MODE_2ASK,
  APP_DAC_WAVE_MODE_2PSK,
  APP_DAC_WAVE_MODE_2FSK,
  APP_DAC_WAVE_MODE_CW,
} AppDacWavegenMode;

#define APP_DAC_WAVE_DEFAULT_MODE               APP_DAC_WAVE_MODE_AM
#define APP_DAC_WAVE_DEFAULT_VPP_MV             300U
#define APP_DAC_WAVE_DEFAULT_MOD_FREQ_HZ        10000UL
#define APP_DAC_WAVE_DEFAULT_SYMBOL_RATE_BPS    10000UL
#define APP_DAC_WAVE_DEFAULT_AM_DEPTH_PERCENT   50U
#define APP_DAC_WAVE_DEFAULT_FM_DEVIATION_HZ    75000UL
#define APP_DAC_WAVE_DEFAULT_FSK_SHIFT_HZ       20000UL

typedef struct
{
  AppDacWavegenMode mode;
  /* 输出端允许的峰峰值，内部实际使用峰值 vpp_mv / 2 参与 I/Q 映射。 */
  uint16_t vpp_mv;
  uint32_t mod_freq_hz;
  uint32_t symbol_rate_bps;
  uint16_t am_depth_percent;
  uint32_t fm_deviation_hz;
  uint32_t fsk_shift_hz;
} AppDacWavegenConfig;

typedef struct
{
  AppDacWavegenConfig config;
  uint8_t running;
  int32_t last_error;
  uint32_t half_irq_count;
  uint32_t full_irq_count;
} AppDacWavegenStatus;

void App_DacWavegenSetDefaultConfig(AppDacWavegenConfig *config);
int App_DacWavegenSetConfig(const AppDacWavegenConfig *config);
void App_DacWavegenGetStatus(AppDacWavegenStatus *status);
int App_DacWavegenStart(void);
void App_DacWavegenStop(void);
void App_DacWavegenTaskRun(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* APP_DAC_WAVEGEN_H */
