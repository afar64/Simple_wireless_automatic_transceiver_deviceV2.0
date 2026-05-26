#ifndef APP_CW_AMP_CAL_H
#define APP_CW_AMP_CAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_CW_AMP_CAL_FREQ_MIN_HZ 110000000UL
#define APP_CW_AMP_CAL_FREQ_MAX_HZ 130000000UL
#define APP_CW_AMP_CAL_FREQ_STEP_HZ 100000UL
#define APP_CW_AMP_CAL_TARGET_MIN_MVRMS 10U
#define APP_CW_AMP_CAL_TARGET_MAX_MVRMS 100U
#define APP_CW_AMP_CAL_TARGET_STEP_MVRMS 10U
#define APP_CW_AMP_CAL_FREQ_COUNT 201U
#define APP_CW_AMP_CAL_TARGET_COUNT 10U

typedef struct
{
  uint16_t amp_code;
  uint16_t atten_db_x10;
  uint16_t measured_mvrms_x10;
} AppCwAmpCalResult;

int App_CwAmpCal_Lookup(uint32_t freq_hz, uint16_t target_mvrms, AppCwAmpCalResult *result);

#ifdef __cplusplus
}
#endif

#endif /* APP_CW_AMP_CAL_H */
