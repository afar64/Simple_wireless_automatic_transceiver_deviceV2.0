#ifndef APP_PE4302_H
#define APP_PE4302_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_PE4302_MAX_HALF_DB_STEPS 63u
#define APP_PE4302_MAX_DB_X10 315u

void App_Pe4302_Init(void);
void App_Pe4302_SetHalfDbSteps(uint8_t steps);
uint8_t App_Pe4302_GetHalfDbSteps(void);
void App_Pe4302_SetDbTenths(uint16_t db_x10);
uint16_t App_Pe4302_GetDbTenths(void);

#ifdef __cplusplus
}
#endif

#endif
