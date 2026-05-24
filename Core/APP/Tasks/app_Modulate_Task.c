#include "cmsis_os2.h"

#include "Modulate.h"

/*
 * 调制相关 RTOS 入口统一放在这个任务文件里：
 * 1. Modulate 任务负责 DAC 基带调制调度；
 * 2. ADC 接收任务单独放在 app_adc_task.c，避免调制侧职责扩散；
 * 3. SI5351 独立任务当前不再承担本振控制，避免与 DDSTask 形成双控制链。
 */
void StartModulateTask(void *argument)
{
  App_ModulateTaskRun(argument);
}

void StartSI5351Task(void *argument)
{
  (void)argument;

  for (;;)
  {
    osDelay(1000U);
  }
}
