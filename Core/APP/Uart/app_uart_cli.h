#ifndef APP_UART_CLI_H
#define APP_UART_CLI_H

#ifdef __cplusplus
extern "C" {
#endif

void App_UartCliTask(void *argument);
void App_UartCliUsart1IrqHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_UART_CLI_H */
