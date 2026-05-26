#include "app_pe4302.h"

#include "main.h"

static uint8_t s_pe4302_steps = 0u;

static void App_Pe4302Delay(void)
{
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  __NOP();
  __NOP();
}

static void App_Pe4302WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
  HAL_GPIO_WritePin(port, pin, state);
  App_Pe4302Delay();
}

static void App_Pe4302WriteRaw(uint8_t steps)
{
  uint8_t bit_index;
  uint8_t value = steps & APP_PE4302_MAX_HALF_DB_STEPS;

  App_Pe4302WritePin(PE4302_CLK_GPIO_Port, PE4302_CLK_Pin, GPIO_PIN_RESET);
  App_Pe4302WritePin(PE4302_LE_GPIO_Port, PE4302_LE_Pin, GPIO_PIN_SET);

  for (bit_index = 0u; bit_index < 6u; ++bit_index)
  {
    GPIO_PinState bit_state = ((value & 0x20u) != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    App_Pe4302WritePin(PE4302_DATA_GPIO_Port, PE4302_DATA_Pin, bit_state);
    App_Pe4302WritePin(PE4302_CLK_GPIO_Port, PE4302_CLK_Pin, GPIO_PIN_SET);
    value = (uint8_t)((value << 1u) & APP_PE4302_MAX_HALF_DB_STEPS);
    App_Pe4302WritePin(PE4302_CLK_GPIO_Port, PE4302_CLK_Pin, GPIO_PIN_RESET);
  }

  App_Pe4302WritePin(PE4302_LE_GPIO_Port, PE4302_LE_Pin, GPIO_PIN_RESET);
  App_Pe4302WritePin(PE4302_DATA_GPIO_Port, PE4302_DATA_Pin, GPIO_PIN_RESET);
}

void App_Pe4302_Init(void)
{
  App_Pe4302WritePin(PE4302_LE_GPIO_Port, PE4302_LE_Pin, GPIO_PIN_SET);
  App_Pe4302WritePin(PE4302_CLK_GPIO_Port, PE4302_CLK_Pin, GPIO_PIN_SET);
  App_Pe4302WritePin(PE4302_DATA_GPIO_Port, PE4302_DATA_Pin, GPIO_PIN_SET);
  App_Pe4302_SetHalfDbSteps(0u);
}

void App_Pe4302_SetHalfDbSteps(uint8_t steps)
{
  if (steps > APP_PE4302_MAX_HALF_DB_STEPS)
  {
    steps = APP_PE4302_MAX_HALF_DB_STEPS;
  }

  s_pe4302_steps = steps;
  App_Pe4302WriteRaw(steps);
}

uint8_t App_Pe4302_GetHalfDbSteps(void)
{
  return s_pe4302_steps;
}

void App_Pe4302_SetDbTenths(uint16_t db_x10)
{
  uint32_t steps;

  if (db_x10 > APP_PE4302_MAX_DB_X10)
  {
    db_x10 = APP_PE4302_MAX_DB_X10;
  }

  steps = ((uint32_t)db_x10 + 2u) / 5u;
  App_Pe4302_SetHalfDbSteps((uint8_t)steps);
}

uint16_t App_Pe4302_GetDbTenths(void)
{
  return (uint16_t)(s_pe4302_steps * 5u);
}
