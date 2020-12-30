#ifndef __MBPORT_H__
#define __MBPORT_H__

#include "stm32f7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

//Максимальное количество обслуживаемых портов (UART)
#define MB_RTU_PORT_NUM_MAX		5
//Аппаратный расчет контрольной суммы
#define MB_RTU_HARDWARE_CRC		1

#endif  /* __MBPORT_H__ */

