#include "app_uart_cli.h"

#include "app_dds_ctrl.h"
#include "app_pe4302.h"
#include "app_tx_control.h"
#include "app_winner_bridge.h"
#include "main.h"
#include "usart.h"

#include "FreeRTOS.h"
#include "task.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_UART_CLI_LINE_MAX      128U
#define APP_UART_CLI_RX_BUF_SIZE   256U
#define APP_UART_CLI_UART_TIMEOUT  100U

static TaskHandle_t s_uart_cli_task_handle = NULL;
static uint8_t s_uart_cli_rx_buf[APP_UART_CLI_RX_BUF_SIZE];
static volatile uint16_t s_uart_cli_rx_head = 0U;
static volatile uint16_t s_uart_cli_rx_tail = 0U;

static void App_UartCliWrite(const char *text)
{
  if (text == NULL)
  {
    return;
  }

  (void)HAL_UART_Transmit(&huart1,
                          (uint8_t *)text,
                          (uint16_t)strlen(text),
                          APP_UART_CLI_UART_TIMEOUT);
}

static void App_UartCliWriteFmt(const char *fmt, uint32_t a, uint32_t b)
{
  char out[96];
  int len = snprintf(out, sizeof(out), fmt, (unsigned long)a, (unsigned long)b);

  if ((len > 0) && ((uint32_t)len < sizeof(out)))
  {
    App_UartCliWrite(out);
  }
}

static void App_UartCliClearErrors(void)
{
  __HAL_UART_CLEAR_OREFLAG(&huart1);
  __HAL_UART_CLEAR_FEFLAG(&huart1);
  __HAL_UART_CLEAR_NEFLAG(&huart1);
  __HAL_UART_CLEAR_PEFLAG(&huart1);
  huart1.ErrorCode = HAL_UART_ERROR_NONE;
}

static void App_UartCliPushByteFromIsr(uint8_t ch)
{
  uint16_t next_head = (uint16_t)((s_uart_cli_rx_head + 1U) % APP_UART_CLI_RX_BUF_SIZE);
  BaseType_t higher_priority_woken = pdFALSE;

  if (next_head != s_uart_cli_rx_tail)
  {
    s_uart_cli_rx_buf[s_uart_cli_rx_head] = ch;
    s_uart_cli_rx_head = next_head;
  }

  if (s_uart_cli_task_handle != NULL)
  {
    vTaskNotifyGiveFromISR(s_uart_cli_task_handle, &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
  }
}

static uint8_t App_UartCliReadByte(uint8_t *ch)
{
  uint8_t has_data = 0U;

  if (ch == NULL)
  {
    return 0U;
  }

  taskENTER_CRITICAL();
  if (s_uart_cli_rx_head != s_uart_cli_rx_tail)
  {
    *ch = s_uart_cli_rx_buf[s_uart_cli_rx_tail];
    s_uart_cli_rx_tail = (uint16_t)((s_uart_cli_rx_tail + 1U) % APP_UART_CLI_RX_BUF_SIZE);
    has_data = 1U;
  }
  taskEXIT_CRITICAL();

  return has_data;
}

static void App_UartCliEnableRxInterrupts(void)
{
  App_UartCliClearErrors();
  __HAL_UART_SEND_REQ(&huart1, UART_RXDATA_FLUSH_REQUEST);
  __HAL_UART_ENABLE_IT(&huart1, UART_IT_PE);
  __HAL_UART_ENABLE_IT(&huart1, UART_IT_ERR);
  __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
}

static void App_UartCliUpper(char *line)
{
  while ((line != NULL) && (*line != '\0'))
  {
    *line = (char)toupper((unsigned char)*line);
    line++;
  }
}

static void App_UartCliWriteDdsStatus(void)
{
  const AppDdsStatus *st = AppDDS_GetStatus();

  App_UartCliWriteFmt("DDS CH0 FREQ %lu AMP %lu\r\n", st->freq_hz[0], st->amp_code[0]);
  App_UartCliWriteFmt("DDS CH1 FREQ %lu AMP %lu\r\n", st->freq_hz[1], st->amp_code[1]);
  App_UartCliWriteFmt("DDS CH2 FREQ %lu AMP %lu\r\n", st->freq_hz[2], st->amp_code[2]);
  App_UartCliWriteFmt("DDS CH3 FREQ %lu AMP %lu\r\n", st->freq_hz[3], st->amp_code[3]);
}

static const char *App_UartCliModeName(AppDacWavegenMode mode)
{
  switch (mode)
  {
    case APP_DAC_WAVE_MODE_AM:
      return "AM";
    case APP_DAC_WAVE_MODE_FM:
      return "FM";
    case APP_DAC_WAVE_MODE_2ASK:
      return "2ASK";
    case APP_DAC_WAVE_MODE_2PSK:
      return "2PSK";
    case APP_DAC_WAVE_MODE_2FSK:
      return "2FSK";
    case APP_DAC_WAVE_MODE_CW:
      return "CW";
    default:
      return "UNKNOWN";
  }
}

static void App_UartCliWriteSweepStatus(void)
{
  AppTxControlSnapshot snapshot;
  char out[192];
  int len;

  App_TxControl_GetSnapshot(&snapshot);
  len = snprintf(out,
                 sizeof(out),
                 "SWEEP MODE %s START %lu STOP %lu PERIOD_MS %lu ENABLE %u CURRENT %lu REL1 %u\r\n",
                 App_UartCliModeName(snapshot.basic.mode),
                 (unsigned long)App_TxControl_GetSweepStartHz(),
                 (unsigned long)App_TxControl_GetSweepStopHz(),
                 (unsigned long)App_TxControl_GetSweepPeriodMs(),
                 (unsigned)snapshot.basic.sweep_on,
                 (unsigned long)snapshot.basic.freq_hz,
                 (unsigned)(HAL_GPIO_ReadPin(REL1_GPIO_Port, REL1_Pin) == GPIO_PIN_SET ? 1U : 0U));
  if ((len > 0) && ((uint32_t)len < sizeof(out)))
  {
    App_UartCliWrite(out);
  }
}

static void App_UartCliWriteDebugStatus(void)
{
  App_UartCliWriteFmt("DEBUG MODE %lu\r\n",
                      (uint32_t)App_TxControl_GetDebugModeEnabled(),
                      0U);
}

static void App_UartCliWriteAttenStatus(void)
{
  char out[64];
  uint16_t db_x10 = App_Pe4302_GetDbTenths();
  uint32_t whole = (uint32_t)(db_x10 / 10U);
  uint32_t frac = (uint32_t)(db_x10 % 10U);
  int len = snprintf(out, sizeof(out), "ATTEN %lu.%01lu dB\r\n",
                     (unsigned long)whole,
                     (unsigned long)frac);

  if ((len > 0) && ((uint32_t)len < sizeof(out)))
  {
    App_UartCliWrite(out);
  }
}

static uint8_t App_UartCliTryParseDbTenths(const char *text, uint16_t *db_x10)
{
  char *endptr = NULL;
  double value;
  long scaled;

  if ((text == NULL) || (db_x10 == NULL))
  {
    return 0U;
  }

  value = strtod(text, &endptr);
  if ((endptr == text) || (endptr == NULL) || (*endptr != '\0'))
  {
    return 0U;
  }

  if ((value < 0.0) || (value > 31.5))
  {
    return 0U;
  }

  scaled = (long)(value * 10.0 + 0.5);
  if ((scaled < 0L) || (scaled > (long)APP_PE4302_MAX_DB_X10))
  {
    return 0U;
  }

  *db_x10 = (uint16_t)scaled;
  return 1U;
}

static void App_UartCliRunStartupSelfTest(void)
{
  const AppDdsStatus *st = AppDDS_GetStatus();
  char out[128];
  int len;

  if (st != NULL)
  {
    len = snprintf(out,
                   sizeof(out),
                   "SELFTEST AD9959 CH0=%lu/%u CH1=%lu/%u\r\n",
                   (unsigned long)st->freq_hz[0],
                   (unsigned)st->amp_code[0],
                   (unsigned long)st->freq_hz[1],
                   (unsigned)st->amp_code[1]);
    if ((len > 0) && ((uint32_t)len < sizeof(out)))
    {
      App_UartCliWrite(out);
    }
    App_UartCliWrite("SELFTEST AD9959 OK\r\n");
  }
  else
  {
    App_UartCliWrite("SELFTEST AD9959 FAIL\r\n");
  }

  if (App_WinnerBridge_RunSelfTest() == 0)
  {
    App_UartCliWrite("SELFTEST F429 OK\r\n");
  }
  else
  {
    App_UartCliWrite("SELFTEST F429 FAIL\r\n");
  }
}

static uint8_t App_UartCliHandleDdsSetChannel(const char *line, uint8_t ch)
{
  unsigned long freq_hz = 0UL;
  unsigned long amp_code = 0UL;
  int ret;
  AppDdsCmd cmd;
  char pattern[40];
  char ok_text[16];

  if (line == NULL)
  {
    return 0U;
  }

  if (ch > 3U)
  {
    return 0U;
  }

  (void)snprintf(pattern, sizeof(pattern), "DDS CH%u FREQ %%lu AMP %%lu", (unsigned)ch);
  if (sscanf(line, pattern, &freq_hz, &amp_code) != 2)
  {
    return 0U;
  }

  if (amp_code > 1023UL)
  {
    App_UartCliWrite("ERR amp out of range\r\n");
    return 1U;
  }

  cmd = AppDDS_MakeSelectChCmd(ch);
  ret = AppDDS_DispatchCmd(&cmd);
  if (ret != 0)
  {
    App_UartCliWrite("ERR dds select\r\n");
    return 1U;
  }

  cmd = AppDDS_MakeSetFreqCmd((uint32_t)freq_hz);
  ret = AppDDS_DispatchCmd(&cmd);
  if (ret != 0)
  {
    App_UartCliWrite("ERR dds freq\r\n");
    return 1U;
  }

  cmd = AppDDS_MakeSetAmpCmd((uint16_t)amp_code);
  ret = AppDDS_DispatchCmd(&cmd);
  if (ret != 0)
  {
    App_UartCliWrite("ERR dds amp\r\n");
    return 1U;
  }

  cmd = AppDDS_MakeApplyCmd();
  ret = AppDDS_DispatchCmd(&cmd);
  if (ret != 0)
  {
    App_UartCliWrite("ERR dds apply\r\n");
    return 1U;
  }

  (void)snprintf(ok_text, sizeof(ok_text), "OK CH%u\r\n", (unsigned)ch);
  App_UartCliWrite(ok_text);
  return 1U;
}

static uint8_t App_UartCliHandleTxAm(const char *line)
{
  unsigned long lo_hz = 0UL;
  unsigned long lo_amp = 0UL;
  unsigned long qg = 0UL;
  long qp = 0L;
  long io = 0L;
  long qo = 0L;
  unsigned long fm = 0UL;
  unsigned long offset = 0UL;
  unsigned long amp = 0UL;
  unsigned long depth = 0UL;
  int ret;

  if (line == NULL)
  {
    return 0U;
  }

  if (sscanf(line,
             "TXAM %lu %lu %lu %ld %ld %ld %lu %lu %lu %lu",
             &lo_hz,
             &lo_amp,
             &qg,
             &qp,
             &io,
             &qo,
             &fm,
             &offset,
             &amp,
             &depth) != 10)
  {
    return 0U;
  }

  if (lo_amp > 1023UL)
  {
    App_UartCliWrite("ERR lo amp out of range\r\n");
    return 1U;
  }

  (void)App_TxControl_SetSweepEnabled(0U);
  (void)App_TxControl_SetMode(APP_DAC_WAVE_MODE_AM);

  ret = App_WinnerBridge_SendAmSequence((uint32_t)lo_hz,
                                        (uint16_t)lo_amp,
                                        (uint16_t)qg,
                                        (int16_t)qp,
                                        (int16_t)io,
                                        (int16_t)qo,
                                        (uint32_t)fm,
                                        (uint16_t)offset,
                                        (uint16_t)amp,
                                        (uint16_t)depth);
  if (ret != 0)
  {
    App_UartCliWrite("ERR txam failed\r\n");
    return 1U;
  }

  App_UartCliWrite("OK TXAM\r\n");
  return 1U;
}

static uint8_t App_UartCliHandleTxFm(const char *line)
{
  unsigned long lo_hz = 0UL;
  unsigned long lo_amp = 0UL;
  unsigned long qg = 0UL;
  long qp = 0L;
  long io = 0L;
  long qo = 0L;
  unsigned long fm = 0UL;
  unsigned long offset = 0UL;
  unsigned long amp = 0UL;
  unsigned long dev = 0UL;
  int ret;

  if (line == NULL)
  {
    return 0U;
  }

  if (sscanf(line,
             "TXFM %lu %lu %lu %ld %ld %ld %lu %lu %lu %lu",
             &lo_hz,
             &lo_amp,
             &qg,
             &qp,
             &io,
             &qo,
             &fm,
             &offset,
             &amp,
             &dev) != 10)
  {
    return 0U;
  }

  if (lo_amp > 1023UL)
  {
    App_UartCliWrite("ERR lo amp out of range\r\n");
    return 1U;
  }

  (void)App_TxControl_SetSweepEnabled(0U);
  (void)App_TxControl_SetMode(APP_DAC_WAVE_MODE_FM);

  ret = App_WinnerBridge_SendFmSequence((uint32_t)lo_hz,
                                        (uint16_t)lo_amp,
                                        (uint16_t)qg,
                                        (int16_t)qp,
                                        (int16_t)io,
                                        (int16_t)qo,
                                        (uint32_t)fm,
                                        (uint16_t)offset,
                                        (uint16_t)amp,
                                        (uint32_t)dev);
  if (ret != 0)
  {
    App_UartCliWrite("ERR txfm failed\r\n");
    return 1U;
  }

  App_UartCliWrite("OK TXFM\r\n");
  return 1U;
}

static uint8_t App_UartCliHandleTxAsk(const char *line)
{
  unsigned long lo_hz = 0UL;
  unsigned long lo_amp = 0UL;
  unsigned long qg = 0UL;
  long qp = 0L;
  long io = 0L;
  long qo = 0L;
  unsigned long br = 0UL;
  unsigned long offset = 0UL;
  unsigned long amp = 0UL;
  unsigned long depth = 0UL;
  int ret;

  if (line == NULL)
  {
    return 0U;
  }

  if (sscanf(line,
             "TXASK %lu %lu %lu %ld %ld %ld %lu %lu %lu %lu",
             &lo_hz,
             &lo_amp,
             &qg,
             &qp,
             &io,
             &qo,
             &br,
             &offset,
             &amp,
             &depth) != 10)
  {
    return 0U;
  }

  if (lo_amp > 1023UL)
  {
    App_UartCliWrite("ERR lo amp out of range\r\n");
    return 1U;
  }

  (void)App_TxControl_SetSweepEnabled(0U);
  (void)App_TxControl_SetMode(APP_DAC_WAVE_MODE_2ASK);

  ret = App_WinnerBridge_SendAskSequence((uint32_t)lo_hz,
                                         (uint16_t)lo_amp,
                                         (uint16_t)qg,
                                         (int16_t)qp,
                                         (int16_t)io,
                                         (int16_t)qo,
                                         (uint32_t)br,
                                         (uint16_t)offset,
                                         (uint16_t)amp,
                                         (uint16_t)depth);
  if (ret != 0)
  {
    App_UartCliWrite("ERR txask failed\r\n");
    return 1U;
  }

  App_UartCliWrite("OK TXASK\r\n");
  return 1U;
}

static uint8_t App_UartCliHandleTxFsk(const char *line)
{
  unsigned long lo_hz = 0UL;
  unsigned long lo_amp = 0UL;
  unsigned long qg = 0UL;
  long qp = 0L;
  long io = 0L;
  long qo = 0L;
  unsigned long br = 0UL;
  unsigned long offset = 0UL;
  unsigned long amp = 0UL;
  unsigned long shift = 0UL;
  int ret;

  if (line == NULL)
  {
    return 0U;
  }

  if (sscanf(line,
             "TXFSK %lu %lu %lu %ld %ld %ld %lu %lu %lu %lu",
             &lo_hz,
             &lo_amp,
             &qg,
             &qp,
             &io,
             &qo,
             &br,
             &offset,
             &amp,
             &shift) != 10)
  {
    return 0U;
  }

  if (lo_amp > 1023UL)
  {
    App_UartCliWrite("ERR lo amp out of range\r\n");
    return 1U;
  }

  (void)App_TxControl_SetSweepEnabled(0U);
  (void)App_TxControl_SetMode(APP_DAC_WAVE_MODE_2FSK);

  ret = App_WinnerBridge_SendFskSequence((uint32_t)lo_hz,
                                         (uint16_t)lo_amp,
                                         (uint16_t)qg,
                                         (int16_t)qp,
                                         (int16_t)io,
                                         (int16_t)qo,
                                         (uint32_t)br,
                                         (uint16_t)offset,
                                         (uint16_t)amp,
                                         (uint32_t)shift);
  if (ret != 0)
  {
    App_UartCliWrite("ERR txfsk failed\r\n");
    return 1U;
  }

  App_UartCliWrite("OK TXFSK\r\n");
  return 1U;
}

static uint8_t App_UartCliHandleTxPsk(const char *line)
{
  unsigned long lo_hz = 0UL;
  unsigned long lo_amp = 0UL;
  unsigned long qg = 0UL;
  long qp = 0L;
  long io = 0L;
  long qo = 0L;
  unsigned long br = 0UL;
  unsigned long offset = 0UL;
  unsigned long amp = 0UL;
  int ret;

  if (line == NULL)
  {
    return 0U;
  }

  if (sscanf(line,
             "TXPSK %lu %lu %lu %ld %ld %ld %lu %lu %lu",
             &lo_hz,
             &lo_amp,
             &qg,
             &qp,
             &io,
             &qo,
             &br,
             &offset,
             &amp) != 9)
  {
    return 0U;
  }

  if (lo_amp > 1023UL)
  {
    App_UartCliWrite("ERR lo amp out of range\r\n");
    return 1U;
  }

  (void)App_TxControl_SetSweepEnabled(0U);
  (void)App_TxControl_SetMode(APP_DAC_WAVE_MODE_2PSK);

  ret = App_WinnerBridge_SendPskSequence((uint32_t)lo_hz,
                                         (uint16_t)lo_amp,
                                         (uint16_t)qg,
                                         (int16_t)qp,
                                         (int16_t)io,
                                         (int16_t)qo,
                                         (uint32_t)br,
                                         (uint16_t)offset,
                                         (uint16_t)amp);
  if (ret != 0)
  {
    App_UartCliWrite("ERR txpsk failed\r\n");
    return 1U;
  }

  App_UartCliWrite("OK TXPSK\r\n");
  return 1U;
}

static uint8_t App_UartCliHandleSweepSet(const char *line)
{
  unsigned long start_hz = 0UL;
  unsigned long stop_hz = 0UL;
  unsigned long period_ms = 0UL;

  if (line == NULL)
  {
    return 0U;
  }

  if (sscanf(line, "SWEEP SET START %lu STOP %lu PERIOD_MS %lu", &start_hz, &stop_hz, &period_ms) != 3)
  {
    return 0U;
  }

  (void)App_TxControl_SetSweepEnabled(0U);
  (void)App_TxControl_SetSweepRangeHz((uint32_t)start_hz, (uint32_t)stop_hz);
  (void)App_TxControl_SetSweepPeriodMs((uint32_t)period_ms);
  App_UartCliWrite("OK SWEEP SET\r\n");
  return 1U;
}

static uint8_t App_UartCliHandleSweepOn(const char *line)
{
  if ((line == NULL) || (strcmp(line, "SWEEP ON") != 0))
  {
    return 0U;
  }

  (void)App_TxControl_SetMode(APP_DAC_WAVE_MODE_CW);
  (void)App_TxControl_Apply();
  (void)App_TxControl_Start();
  (void)App_TxControl_SetSweepEnabled(1U);
  App_UartCliWrite("OK SWEEP ON\r\n");
  return 1U;
}

static uint8_t App_UartCliHandleSweepOff(const char *line)
{
  if ((line == NULL) || (strcmp(line, "SWEEP OFF") != 0))
  {
    return 0U;
  }

  (void)App_TxControl_SetSweepEnabled(0U);
  App_UartCliWrite("OK SWEEP OFF\r\n");
  return 1U;
}

static uint8_t App_UartCliHandleSweepQuery(const char *line)
{
  if ((line == NULL) || (strcmp(line, "SWEEP?") != 0))
  {
    return 0U;
  }

  App_UartCliWriteSweepStatus();
  return 1U;
}

static uint8_t App_UartCliHandleDebugSet(const char *line)
{
  if (line == NULL)
  {
    return 0U;
  }

  if (strcmp(line, "DEBUG ON") == 0)
  {
    (void)App_TxControl_SetDebugModeEnabled(1U);
    App_UartCliWrite("OK DEBUG ON\r\n");
    return 1U;
  }

  if (strcmp(line, "DEBUG OFF") == 0)
  {
    (void)App_TxControl_SetDebugModeEnabled(0U);
    App_UartCliWrite("OK DEBUG OFF\r\n");
    return 1U;
  }

  if (strcmp(line, "DEBUG?") == 0)
  {
    App_UartCliWriteDebugStatus();
    return 1U;
  }

  return 0U;
}

static uint8_t App_UartCliHandleSelfTest(const char *line)
{
  if ((line == NULL) || (strcmp(line, "SELFTEST") != 0))
  {
    return 0U;
  }

  App_UartCliRunStartupSelfTest();
  return 1U;
}

static uint8_t App_UartCliHandleAtten(const char *line)
{
  const char *value_text = NULL;
  uint16_t db_x10 = 0U;

  if (line == NULL)
  {
    return 0U;
  }

  if (strcmp(line, "ATTEN?") == 0)
  {
    App_UartCliWriteAttenStatus();
    return 1U;
  }

  if (strncmp(line, "ATTEN ", 6) != 0)
  {
    return 0U;
  }

  value_text = line + 6;
  if (App_UartCliTryParseDbTenths(value_text, &db_x10) == 0U)
  {
    App_UartCliWrite("ERR atten range 0.0~31.5 step 0.5\r\n");
    return 1U;
  }

  if ((db_x10 % 5U) != 0U)
  {
    App_UartCliWrite("ERR atten range 0.0~31.5 step 0.5\r\n");
    return 1U;
  }

  App_Pe4302_SetDbTenths(db_x10);
  App_UartCliWrite("OK ATTEN\r\n");
  return 1U;
}

static void App_UartCliHandleCommand(char *line)
{
  if (line == NULL)
  {
    return;
  }

  App_UartCliUpper(line);

  if ((strcmp(line, "HELP") == 0) || (strcmp(line, "?") == 0))
  {
    App_UartCliWrite("HELP\r\n");
    App_UartCliWrite("PING\r\n");
    App_UartCliWrite("DDS?\r\n");
    App_UartCliWrite("DDS CH0 FREQ <HZ> AMP <CODE>\r\n");
    App_UartCliWrite("DDS CH1 FREQ <HZ> AMP <CODE>\r\n");
    App_UartCliWrite("SWEEP SET START <HZ> STOP <HZ> PERIOD_MS <MS>\r\n");
    App_UartCliWrite("SWEEP ON\r\n");
    App_UartCliWrite("SWEEP OFF\r\n");
    App_UartCliWrite("SWEEP?\r\n");
    App_UartCliWrite("DEBUG ON\r\n");
    App_UartCliWrite("DEBUG OFF\r\n");
    App_UartCliWrite("DEBUG?\r\n");
    App_UartCliWrite("ATTEN <DB>\r\n");
    App_UartCliWrite("ATTEN?\r\n");
    App_UartCliWrite("SELFTEST\r\n");
    App_UartCliWrite("TXAM <LO_HZ> <LO_AMP> <QG> <QP> <IO> <QO> <FM> <OFFSET> <AMP> <DEPTH>\r\n");
    App_UartCliWrite("TXFM <LO_HZ> <LO_AMP> <QG> <QP> <IO> <QO> <FM> <OFFSET> <AMP> <DEV>\r\n");
    App_UartCliWrite("TXASK <LO_HZ> <LO_AMP> <QG> <QP> <IO> <QO> <BR> <OFFSET> <AMP> <DEPTH>\r\n");
    App_UartCliWrite("TXFSK <LO_HZ> <LO_AMP> <QG> <QP> <IO> <QO> <BR> <OFFSET> <AMP> <SHIFT>\r\n");
    App_UartCliWrite("TXPSK <LO_HZ> <LO_AMP> <QG> <QP> <IO> <QO> <BR> <OFFSET> <AMP>\r\n");
    return;
  }

  if (strcmp(line, "PING") == 0)
  {
    App_UartCliWrite("PONG\r\n");
    return;
  }

  if (strcmp(line, "DDS?") == 0)
  {
    App_UartCliWriteDdsStatus();
    return;
  }

  if (App_UartCliHandleDdsSetChannel(line, 0U) != 0U)
  {
    return;
  }

  if (App_UartCliHandleDdsSetChannel(line, 1U) != 0U)
  {
    return;
  }

  if (App_UartCliHandleSweepSet(line) != 0U)
  {
    return;
  }

  if (App_UartCliHandleSweepOn(line) != 0U)
  {
    return;
  }

  if (App_UartCliHandleSweepOff(line) != 0U)
  {
    return;
  }

  if (App_UartCliHandleSweepQuery(line) != 0U)
  {
    return;
  }

  if (App_UartCliHandleDebugSet(line) != 0U)
  {
    return;
  }

  if (App_UartCliHandleAtten(line) != 0U)
  {
    return;
  }

  if (App_UartCliHandleSelfTest(line) != 0U)
  {
    return;
  }

  if (App_UartCliHandleTxAm(line) != 0U)
  {
    return;
  }

  if (App_UartCliHandleTxFm(line) != 0U)
  {
    return;
  }

  if (App_UartCliHandleTxAsk(line) != 0U)
  {
    return;
  }

  if (App_UartCliHandleTxFsk(line) != 0U)
  {
    return;
  }

  if (App_UartCliHandleTxPsk(line) != 0U)
  {
    return;
  }

  App_UartCliWrite("ERR unknown command\r\n");
}

void App_UartCliTask(void *argument)
{
  uint8_t ch;
  char line[APP_UART_CLI_LINE_MAX];
  uint32_t len = 0U;

  (void)argument;

  s_uart_cli_task_handle = xTaskGetCurrentTaskHandle();
  s_uart_cli_rx_head = 0U;
  s_uart_cli_rx_tail = 0U;
  App_UartCliEnableRxInterrupts();

  App_UartCliWrite("\r\nUSART1 CLI ready @ 115200\r\n");
  App_UartCliWrite("Type HELP\r\n");
  App_UartCliRunStartupSelfTest();

  for (;;)
  {
    if (App_UartCliReadByte(&ch) == 0U)
    {
      (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20U));
      continue;
    }

    if ((ch == '\r') || (ch == '\n'))
    {
      if (len != 0U)
      {
        line[len] = '\0';
        App_UartCliHandleCommand(line);
        len = 0U;
      }
      continue;
    }

    if (len < (APP_UART_CLI_LINE_MAX - 1U))
    {
      line[len++] = (char)ch;
    }
    else
    {
      len = 0U;
      App_UartCliWrite("ERR line too long\r\n");
    }
  }
}

void App_UartCliUsart1IrqHandler(void)
{
  uint32_t isr = huart1.Instance->ISR;
  uint32_t cr1 = huart1.Instance->CR1;
  uint32_t cr3 = huart1.Instance->CR3;

  if (((isr & UART_FLAG_RXNE) != 0U) && ((cr1 & USART_CR1_RXNEIE_RXFNEIE) != 0U))
  {
    uint8_t ch = (uint8_t)(huart1.Instance->RDR & 0xFFU);
    App_UartCliPushByteFromIsr(ch);
  }

  if (((isr & UART_FLAG_PE) != 0U) && ((cr1 & USART_CR1_PEIE) != 0U))
  {
    __HAL_UART_CLEAR_PEFLAG(&huart1);
  }

  if (((isr & UART_FLAG_FE) != 0U) && ((cr3 & USART_CR3_EIE) != 0U))
  {
    __HAL_UART_CLEAR_FEFLAG(&huart1);
  }

  if (((isr & UART_FLAG_NE) != 0U) && ((cr3 & USART_CR3_EIE) != 0U))
  {
    __HAL_UART_CLEAR_NEFLAG(&huart1);
  }

  if (((isr & UART_FLAG_ORE) != 0U) &&
      (((cr1 & USART_CR1_RXNEIE_RXFNEIE) != 0U) || ((cr3 & USART_CR3_EIE) != 0U)))
  {
    __HAL_UART_CLEAR_OREFLAG(&huart1);
  }
}
