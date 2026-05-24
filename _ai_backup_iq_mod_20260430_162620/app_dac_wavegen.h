#ifndef APP_DAC_WAVEGEN_H
#define APP_DAC_WAVEGEN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_DAC_WAVE_SAMPLE_RATE_HZ 2000000UL
#define APP_DAC_WAVE_FREQ_MIN_HZ    1UL
#define APP_DAC_WAVE_FREQ_MAX_HZ    100000UL
#define APP_DAC_WAVE_MV_MIN         0U
#define APP_DAC_WAVE_MV_MAX         3300U
#define APP_DAC_WAVE_DAC_REF_MV     3531U

typedef struct
{
  uint32_t freq_hz;
  uint16_t low_mv;
  uint16_t high_mv;
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
