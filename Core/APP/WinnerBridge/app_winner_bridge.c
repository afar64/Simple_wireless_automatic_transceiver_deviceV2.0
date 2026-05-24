#include "app_winner_bridge.h"

#include "app_ad9959_task.h"
#include "main.h"
#include "usart.h"

#include <stdio.h>
#include <string.h>

#define APP_WINNER_BRIDGE_UART_TIMEOUT_MS  200U
#define APP_WINNER_BRIDGE_RX_POLL_MS       300U

static volatile uint8_t s_winner_bridge_last_selftest_ok = 0U;

static void App_WinnerBridge_DebugWrite(const char *text)
{
  if ((text == NULL) || (text[0] == '\0'))
  {
    return;
  }

  (void)HAL_UART_Transmit(&huart1,
                          (uint8_t *)text,
                          (uint16_t)strlen(text),
                          APP_WINNER_BRIDGE_UART_TIMEOUT_MS);
}

static uint16_t App_WinnerBridge_SendLineAndMirror(const char *line)
{
  uint32_t start_tick;
  uint8_t rx_buf[192];
  uint16_t rx_len = 0U;

  if ((line == NULL) || (line[0] == '\0'))
  {
    return 0U;
  }

  App_WinnerBridge_DebugWrite("[WB] USART3 send: ");
  App_WinnerBridge_DebugWrite(line);

  (void)HAL_UART_Transmit(&huart3,
                          (uint8_t *)line,
                          (uint16_t)strlen(line),
                          APP_WINNER_BRIDGE_UART_TIMEOUT_MS);

  start_tick = HAL_GetTick();
  while ((HAL_GetTick() - start_tick) < APP_WINNER_BRIDGE_RX_POLL_MS)
  {
    while (((huart3.Instance->ISR & USART_ISR_RXNE_RXFNE) != 0U) &&
           (rx_len < (uint16_t)sizeof(rx_buf)))
    {
      rx_buf[rx_len++] = (uint8_t)huart3.Instance->RDR;
    }
  }

  if (rx_len != 0U)
  {
    App_WinnerBridge_DebugWrite("[WB] USART3 reply: ");
    (void)HAL_UART_Transmit(&huart1, rx_buf, rx_len, APP_WINNER_BRIDGE_UART_TIMEOUT_MS);
    App_WinnerBridge_DebugWrite("\r\n");
  }
  else
  {
    App_WinnerBridge_DebugWrite("[WB] USART3 reply: <none>\r\n");
  }

  return rx_len;
}

static int App_WinnerBridge_SetLoAndCommonIq(uint32_t lo_freq_hz,
                                             uint16_t lo_amp_code,
                                             uint16_t q_gain_permille,
                                             int16_t q_phase_deg,
                                             int16_t i_trim,
                                             int16_t q_trim)
{
  char line[64];
  int len;
  uint16_t applied_lo_amp_code;

  (void)lo_amp_code;

  /* Non-CW modulation path uses CH0 as LO and forces full-scale DDS amplitude. */
  applied_lo_amp_code = 1023U;

  (void)App_LoTaskSetChannelEnable(APP_AD9959_TASK_CW_CHANNEL, 0U);

  if (App_LoTaskSetChannelFrequencyHz(APP_AD9959_TASK_MOD_CHANNEL, lo_freq_hz) != 0)
  {
    App_WinnerBridge_DebugWrite("[WB] ERR local CH0 freq\r\n");
    return -1;
  }

  if (App_LoTaskSetChannelAmplitudeCode(APP_AD9959_TASK_MOD_CHANNEL, applied_lo_amp_code) != 0)
  {
    App_WinnerBridge_DebugWrite("[WB] ERR local CH0 amp\r\n");
    return -2;
  }

  if (App_LoTaskSetChannelEnable(APP_AD9959_TASK_MOD_CHANNEL, 1U) != 0)
  {
    App_WinnerBridge_DebugWrite("[WB] ERR local CH0 enable\r\n");
    return -3;
  }

  len = snprintf(line, sizeof(line), "\r\n[WB] CH0 LO set: %lu Hz AMP %u\r\n",
                 (unsigned long)lo_freq_hz,
                 (unsigned)applied_lo_amp_code);
  if ((len > 0) && ((uint32_t)len < sizeof(line)))
  {
    App_WinnerBridge_DebugWrite(line);
  }

  len = snprintf(line, sizeof(line), "QG %u\r\n", (unsigned)q_gain_permille);
  if ((len <= 0) || ((uint32_t)len >= sizeof(line)))
  {
    return -4;
  }
  App_WinnerBridge_SendLineAndMirror(line);

  len = snprintf(line, sizeof(line), "QP %d\r\n", (int)q_phase_deg);
  if ((len <= 0) || ((uint32_t)len >= sizeof(line)))
  {
    return -5;
  }
  App_WinnerBridge_SendLineAndMirror(line);

  len = snprintf(line, sizeof(line), "IO %d\r\n", (int)i_trim);
  if ((len <= 0) || ((uint32_t)len >= sizeof(line)))
  {
    return -6;
  }
  App_WinnerBridge_SendLineAndMirror(line);

  len = snprintf(line, sizeof(line), "QO %d\r\n", (int)q_trim);
  if ((len <= 0) || ((uint32_t)len >= sizeof(line)))
  {
    return -7;
  }
  App_WinnerBridge_SendLineAndMirror(line);

  return 0;
}

int App_WinnerBridge_SendAmSequence(uint32_t lo_freq_hz,
                                    uint16_t lo_amp_code,
                                    uint16_t q_gain_permille,
                                    int16_t q_phase_deg,
                                    int16_t i_trim,
                                    int16_t q_trim,
                                    uint32_t mod_freq_hz,
                                    uint16_t offset_code,
                                    uint16_t amp_code,
                                    uint16_t depth_permille)
{
  char line[64];
  int len;
  int ret;

  ret = App_WinnerBridge_SetLoAndCommonIq(lo_freq_hz,
                                          lo_amp_code,
                                          q_gain_permille,
                                          q_phase_deg,
                                          i_trim,
                                          q_trim);
  if (ret != 0)
  {
    return ret;
  }

  len = snprintf(line, sizeof(line), "TAM %lu %u %u %u\r\n",
                 (unsigned long)mod_freq_hz,
                 (unsigned)offset_code,
                 (unsigned)amp_code,
                 (unsigned)depth_permille);
  if ((len <= 0) || ((uint32_t)len >= sizeof(line)))
  {
    return -8;
  }
  App_WinnerBridge_SendLineAndMirror(line);

  App_WinnerBridge_DebugWrite("[WB] TXAM done\r\n");
  return 0;
}

int App_WinnerBridge_SendFmSequence(uint32_t lo_freq_hz,
                                    uint16_t lo_amp_code,
                                    uint16_t q_gain_permille,
                                    int16_t q_phase_deg,
                                    int16_t i_trim,
                                    int16_t q_trim,
                                    uint32_t mod_freq_hz,
                                    uint16_t offset_code,
                                    uint16_t amp_code,
                                    uint32_t dev_hz)
{
  char line[64];
  int len;
  int ret;

  ret = App_WinnerBridge_SetLoAndCommonIq(lo_freq_hz,
                                          lo_amp_code,
                                          q_gain_permille,
                                          q_phase_deg,
                                          i_trim,
                                          q_trim);
  if (ret != 0)
  {
    return ret;
  }

  len = snprintf(line, sizeof(line), "TFM %lu %u %u %lu\r\n",
                 (unsigned long)mod_freq_hz,
                 (unsigned)offset_code,
                 (unsigned)amp_code,
                 (unsigned long)dev_hz);
  if ((len <= 0) || ((uint32_t)len >= sizeof(line)))
  {
    return -8;
  }
  App_WinnerBridge_SendLineAndMirror(line);

  App_WinnerBridge_DebugWrite("[WB] TXFM done\r\n");
  return 0;
}

int App_WinnerBridge_SendAskSequence(uint32_t lo_freq_hz,
                                     uint16_t lo_amp_code,
                                     uint16_t q_gain_permille,
                                     int16_t q_phase_deg,
                                     int16_t i_trim,
                                     int16_t q_trim,
                                     uint32_t bit_rate_hz,
                                     uint16_t offset_code,
                                     uint16_t amp_code,
                                     uint16_t depth_permille)
{
  char line[64];
  int len;
  int ret;

  ret = App_WinnerBridge_SetLoAndCommonIq(lo_freq_hz,
                                          lo_amp_code,
                                          q_gain_permille,
                                          q_phase_deg,
                                          i_trim,
                                          q_trim);
  if (ret != 0)
  {
    return ret;
  }

  len = snprintf(line, sizeof(line), "TASK %lu %u %u %u\r\n",
                 (unsigned long)bit_rate_hz,
                 (unsigned)offset_code,
                 (unsigned)amp_code,
                 (unsigned)depth_permille);
  if ((len <= 0) || ((uint32_t)len >= sizeof(line)))
  {
    return -8;
  }
  App_WinnerBridge_SendLineAndMirror(line);

  App_WinnerBridge_DebugWrite("[WB] TXASK done\r\n");
  return 0;
}

int App_WinnerBridge_SendFskSequence(uint32_t lo_freq_hz,
                                     uint16_t lo_amp_code,
                                     uint16_t q_gain_permille,
                                     int16_t q_phase_deg,
                                     int16_t i_trim,
                                     int16_t q_trim,
                                     uint32_t bit_rate_hz,
                                     uint16_t offset_code,
                                     uint16_t amp_code,
                                     uint32_t shift_hz)
{
  char line[64];
  int len;
  int ret;

  ret = App_WinnerBridge_SetLoAndCommonIq(lo_freq_hz,
                                          lo_amp_code,
                                          q_gain_permille,
                                          q_phase_deg,
                                          i_trim,
                                          q_trim);
  if (ret != 0)
  {
    return ret;
  }

  len = snprintf(line, sizeof(line), "TFSK %lu %u %u %lu\r\n",
                 (unsigned long)bit_rate_hz,
                 (unsigned)offset_code,
                 (unsigned)amp_code,
                 (unsigned long)shift_hz);
  if ((len <= 0) || ((uint32_t)len >= sizeof(line)))
  {
    return -8;
  }
  App_WinnerBridge_SendLineAndMirror(line);

  App_WinnerBridge_DebugWrite("[WB] TXFSK done\r\n");
  return 0;
}

int App_WinnerBridge_SendPskSequence(uint32_t lo_freq_hz,
                                     uint16_t lo_amp_code,
                                     uint16_t q_gain_permille,
                                     int16_t q_phase_deg,
                                     int16_t i_trim,
                                     int16_t q_trim,
                                     uint32_t bit_rate_hz,
                                     uint16_t offset_code,
                                     uint16_t amp_code)
{
  char line[64];
  int len;
  int ret;

  ret = App_WinnerBridge_SetLoAndCommonIq(lo_freq_hz,
                                          lo_amp_code,
                                          q_gain_permille,
                                          q_phase_deg,
                                          i_trim,
                                          q_trim);
  if (ret != 0)
  {
    return ret;
  }

  len = snprintf(line, sizeof(line), "TBPSK %lu %u %u\r\n",
                 (unsigned long)bit_rate_hz,
                 (unsigned)offset_code,
                 (unsigned)amp_code);
  if ((len <= 0) || ((uint32_t)len >= sizeof(line)))
  {
    return -8;
  }
  App_WinnerBridge_SendLineAndMirror(line);

  App_WinnerBridge_DebugWrite("[WB] TXPSK done\r\n");
  return 0;
}

int App_WinnerBridge_RunSelfTest(void)
{
  uint16_t rx_len;

  App_WinnerBridge_DebugWrite("[WB] SELFTEST USART3 -> F429\r\n");
  rx_len = App_WinnerBridge_SendLineAndMirror("?\r\n");
  if (rx_len == 0U)
  {
    s_winner_bridge_last_selftest_ok = 0U;
    App_WinnerBridge_DebugWrite("[WB] SELFTEST F429 FAIL\r\n");
    return -1;
  }

  s_winner_bridge_last_selftest_ok = 1U;
  App_WinnerBridge_DebugWrite("[WB] SELFTEST F429 OK\r\n");
  return 0;
}

uint8_t App_WinnerBridge_GetLastSelfTestOk(void)
{
  return s_winner_bridge_last_selftest_ok;
}
