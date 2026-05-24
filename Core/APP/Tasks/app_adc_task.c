#include "cmsis_os2.h"

extern osSemaphoreId_t AdcFrameReadySemHandle;

/*
 * ADC 任务当前只保留独立任务壳：
 * - 已从调制任务中解耦；
 * - 暂不在这里塞入解调、频谱或 DMA 复杂逻辑；
 * - 后续接收链明确后，再单独扩展这个文件。
 */
void StartAdcTask(void *argument)
{
  (void)argument;

  for (;;)
  {
    if (AdcFrameReadySemHandle == NULL)
    {
      osDelay(100U);
      continue;
    }

    (void)osSemaphoreAcquire(AdcFrameReadySemHandle, 100U);
  }
}
