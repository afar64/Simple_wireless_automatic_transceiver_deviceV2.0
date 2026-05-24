#ifndef APP_AD9959_TASK_H
#define APP_AD9959_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Keep the LO policy in task context so DAC only emits a "baseband ready"
 * event and never takes over AD9959/SI5351 ownership.
 */
#define APP_LO_SOURCE_AD9959 1U
#define APP_LO_SOURCE_SI5351 2U

#ifndef APP_LO_SELECTED_SOURCE
#define APP_LO_SELECTED_SOURCE APP_LO_SOURCE_AD9959
#endif

#define APP_LO_FREQ_MIN_HZ           1000000UL
#define APP_LO_FREQ_MAX_HZ         160000000UL
#define APP_LO_FREQ_STEP_MIN_HZ        1000UL

#define APP_AD9959_TASK_MOD_CHANNEL      0U
#define APP_AD9959_TASK_CW_CHANNEL       1U
#define APP_AD9959_TASK_DEFAULT_CHANNEL  APP_AD9959_TASK_MOD_CHANNEL
#define APP_AD9959_TASK_DEFAULT_FREQ_HZ  130000000UL
#define APP_AD9959_TASK_STARTUP_CH1_FREQ_HZ 20000000UL
/* Current project has no verified AD9959 mV-to-code calibration yet. */
#define APP_AD9959_TASK_DEFAULT_AMP_CODE 512U
#define APP_AD9959_TASK_STARTUP_CH1_AMP_CODE 300U

#define APP_SI5351_TASK_DEFAULT_CHANNEL 0U
#define APP_SI5351_TASK_DEFAULT_FREQ_HZ APP_AD9959_TASK_DEFAULT_FREQ_HZ

void App_LoTaskNotifyBasebandReady(void);
uint32_t App_LoTaskGetFrequencyHz(void);
int App_LoTaskSetFrequencyHz(uint32_t freq_hz);
uint32_t App_LoTaskGetChannelFrequencyHz(uint8_t ch);
int App_LoTaskSetChannelFrequencyHz(uint8_t ch, uint32_t freq_hz);
uint16_t App_LoTaskGetChannelAmplitudeCode(uint8_t ch);
int App_LoTaskSetChannelAmplitudeCode(uint8_t ch, uint16_t amp_code);
uint8_t App_LoTaskGetChannelEnable(uint8_t ch);
int App_LoTaskSetChannelEnable(uint8_t ch, uint8_t enable);

#ifdef __cplusplus
}
#endif

#endif /* APP_AD9959_TASK_H */
