#include "logger.h"
#include "config.h"
#include "sys.h"

SystemInfo systemInfo;

/* 每秒定时任务 */
static void system_handle(void *arg)
{
  // 记录CPU信息
  TaskStatus_t *tasks = NULL;
  TaskStatus_t *last_tasks = NULL;
  UBaseType_t tasks_size;
  UBaseType_t last_tasks_size;
  uint32_t tasktime;
  uint32_t last_tasktime;
  last_tasks_size = uxTaskGetNumberOfTasks();
  last_tasks = (TaskStatus_t *)heap_caps_malloc(sizeof(TaskStatus_t) * last_tasks_size, MALLOC_CAP_SPIRAM);
  last_tasks_size = uxTaskGetSystemState(last_tasks, last_tasks_size, &last_tasktime);

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(TASK_SYSTEM_PERIOD);
  while (true)
  {
    // 精准延时
    xTaskDelayUntil(&xLastWakeTime, xFrequency);

    // 写入系统信息
    // CPU信息
    uint64_t idle0 = 0;
    uint64_t idle1 = 0;
    tasks_size = uxTaskGetNumberOfTasks();
    tasks = (TaskStatus_t *)heap_caps_malloc(sizeof(TaskStatus_t) * tasks_size, MALLOC_CAP_SPIRAM);
    tasks_size = uxTaskGetSystemState(tasks, tasks_size, &tasktime);
#ifdef SYSTEM_PRINT_INFORMATION
    logger::debugln("CPU info:\n");
    logger::debugln("  | Task | Run Time | Percentage |\n");
#endif
    for (int i = 0; i < tasks_size; i++)
    {
#ifdef SYSTEM_PRINT_INFORMATION
      uint32_t task_elapsed_time = tasks[i].ulRunTimeCounter;
      uint32_t percentage_time = task_elapsed_time * 100UL / 1000;
      logger::debugln("  | %s | %d | %d |\n", tasks[i].pcTaskName, task_elapsed_time, percentage_time);
#endif
      // 找到空闲任务
      if (strcmp(tasks[i].pcTaskName, "IDLE0") == 0)
      {
        idle0 += tasks[i].ulRunTimeCounter;
      }
      else if (strcmp(tasks[i].pcTaskName, "IDLE1") == 0)
      {
        idle1 += tasks[i].ulRunTimeCounter;
      }
    }
    for (int i = 0; i < last_tasks_size; i++)
    {
      if (strcmp(last_tasks[i].pcTaskName, "IDLE0") == 0)
      {
        idle0 -= last_tasks[i].ulRunTimeCounter;
      }
      else if (strcmp(last_tasks[i].pcTaskName, "IDLE1") == 0)
      {
        idle1 -= last_tasks[i].ulRunTimeCounter;
      }
    }
    systemInfo.cpu0Usage = 100.0 - (idle0 * 100.0 / (tasktime - last_tasktime));
    systemInfo.cpu1Usage = 100.0 - (idle1 * 100.0 / (tasktime - last_tasktime));
    free(last_tasks);
    last_tasks = tasks;
    last_tasks_size = tasks_size;
    last_tasktime = tasktime;
#ifdef SYSTEM_PRINT_INFORMATION
    logger::debugln("  CPU0:%f%, CPU1:%f%\n", systemInfo.cpu0Usage, systemInfo.cpu1Usage);
#endif

    // IRAM内存信息
    multi_heap_info_t heapInfo;
    heap_caps_get_info(&heapInfo, MALLOC_CAP_INTERNAL);
    systemInfo.iramUsedSize = heapInfo.total_allocated_bytes;
    systemInfo.iramTotalSize = systemInfo.iramUsedSize + heapInfo.total_free_bytes;

    // PSRAM内存信息
    heap_caps_get_info(&heapInfo, MALLOC_CAP_SPIRAM);
    systemInfo.psramUsedSize = heapInfo.total_allocated_bytes;
    systemInfo.psramTotalSize = systemInfo.psramUsedSize + heapInfo.total_free_bytes;

#ifdef SYSTEM_PRINT_INFORMATION
    logger::debugln("Memory Info:");
    logger::debugln("  IRAM: %d/%dKB\n", systemInfo.iramUsedSize / 1024, systemInfo.iramTotalSize / 1024);
    logger::debugln("  PSRAM: %d/%dKB\n", systemInfo.psramUsedSize / 1024, systemInfo.psramTotalSize / 1024);
#endif
  }
}

void sys::setup()
{
  esp_reset_reason_t reason = esp_reset_reason();
  switch (reason)
  {
  case ESP_RST_PANIC: //!< Software reset due to exception/panic
  {
    break;
  }

  case ESP_RST_INT_WDT:  //!< Reset (software or hardware) due to interrupt watchdog
  case ESP_RST_TASK_WDT: //!< Reset due to task watchdog
  case ESP_RST_WDT:      //!< Reset due to other watchdogs
  {
    break;
  }

  case ESP_RST_UNKNOWN: //!< Reset reason can not be determined
  {
    break;
  }

  case ESP_RST_EFUSE: //!< Reset due to efuse error
  {
    break;
  }

  case ESP_RST_PWR_GLITCH: //!< Reset due to power glitch detected
  {
    break;
  }

  case ESP_RST_CPU_LOCKUP: //!< Reset due to CPU lock up (double exception)
  {
    break;
  }

  // 不需要提示
  case ESP_RST_POWERON:   //!< Reset due to power-on event
  case ESP_RST_EXT:       //!< Reset by external pin (not applicable for ESP32)
  case ESP_RST_SW:        //!< Software reset via esp_restart
  case ESP_RST_DEEPSLEEP: //!< Reset after exiting deep sleep mode
  case ESP_RST_BROWNOUT:  //!< Brownout reset (software or hardware)
  case ESP_RST_SDIO:      //!< Reset over SDIO
  case ESP_RST_USB:       //!< Reset by USB peripheral
  case ESP_RST_JTAG:      //!< Reset by JTAG
  default:
  {
    break;
  }
  }
  xTaskCreatePinnedToCore(system_handle, "system_handle", TASK_SYSTEM_STACK, NULL, TASK_SYSTEM_PRIORITY, NULL, TASK_SYSTEM_CORE);
}