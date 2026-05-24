#include "cmsis_os2.h"

#include "Modulate.h"
#include "app_dac_wavegen.h"

/*
 * Modulate.c 只负责调制相关调度：
 * - 调制任务启动 DAC 基带 IQ 输出；
 * - 调制参数计算、预生成表、模式切换仍保留在调制链路；
 * - ADC/解调/频谱等接收链逻辑不放在这里，避免职责扩散。
 */
void App_ModulateTaskRun(void *argument)
{
  (void)argument;

  (void)App_DacWavegenStart();

  for (;;)
  {
    osDelay(100U);
  }
}
