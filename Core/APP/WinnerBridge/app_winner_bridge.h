#ifndef APP_WINNER_BRIDGE_H
#define APP_WINNER_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int App_WinnerBridge_SendAmSequence(uint32_t lo_freq_hz,
                                    uint16_t lo_amp_code,
                                    uint16_t q_gain_permille,
                                    int16_t q_phase_deg,
                                    int16_t i_trim,
                                    int16_t q_trim,
                                    uint32_t mod_freq_hz,
                                    uint16_t offset_code,
                                    uint16_t amp_code,
                                    uint16_t depth_permille);
int App_WinnerBridge_SendFmSequence(uint32_t lo_freq_hz,
                                    uint16_t lo_amp_code,
                                    uint16_t q_gain_permille,
                                    int16_t q_phase_deg,
                                    int16_t i_trim,
                                    int16_t q_trim,
                                    uint32_t mod_freq_hz,
                                    uint16_t offset_code,
                                    uint16_t amp_code,
                                    uint32_t dev_hz);
int App_WinnerBridge_SendAskSequence(uint32_t lo_freq_hz,
                                     uint16_t lo_amp_code,
                                     uint16_t q_gain_permille,
                                     int16_t q_phase_deg,
                                     int16_t i_trim,
                                     int16_t q_trim,
                                     uint32_t bit_rate_hz,
                                     uint16_t offset_code,
                                     uint16_t amp_code,
                                     uint16_t depth_permille);
int App_WinnerBridge_SendFskSequence(uint32_t lo_freq_hz,
                                     uint16_t lo_amp_code,
                                     uint16_t q_gain_permille,
                                     int16_t q_phase_deg,
                                     int16_t i_trim,
                                     int16_t q_trim,
                                     uint32_t bit_rate_hz,
                                     uint16_t offset_code,
                                     uint16_t amp_code,
                                     uint32_t shift_hz);
int App_WinnerBridge_SendPskSequence(uint32_t lo_freq_hz,
                                     uint16_t lo_amp_code,
                                     uint16_t q_gain_permille,
                                     int16_t q_phase_deg,
                                     int16_t i_trim,
                                     int16_t q_trim,
                                     uint32_t bit_rate_hz,
                                     uint16_t offset_code,
                                     uint16_t amp_code);
int App_WinnerBridge_RunSelfTest(void);
uint8_t App_WinnerBridge_GetLastSelfTestOk(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_WINNER_BRIDGE_H */
