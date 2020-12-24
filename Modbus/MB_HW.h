#ifndef __MB_HW_H__
#define __MB_HW_H__

#include "stm32f7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "MB.h"

#define MB_HW_ADU_SIZE_MAX      257 //на один байт больше чем 256, чтобы всегда ловить пакет по тайм-ауту
#define MB_HW_ADU_SIZE_MIN      4

//Максимальное количество обслуживаемых портов (UART)
#define MB_HW_PORT_NUM_MAX		5

typedef struct
{
	UART_HandleTypeDef	*uart; //Указатель на структуру UART из HAL
	uint8_t SlaveAddress;
	uint8_t	Buf[MB_HW_ADU_SIZE_MAX];   //Массив для хранения пакета
	uint16_t BufLen; //Размер пакета
	xTaskHandle Handler_Task;  //Задача, которая обрабатывает пакеты
	bool InterFrameTimeout_Fix;//Фикстрованый тайм-аут между пакетами InterFrameTimeout
	uint32_t InterFrameTimeout; //тайм-аут между пакетами в битах, если InterFrameTimeout_Fix = true
} MB_HW_HandleTypeDef;

MB_ErrorRet MB_HW_Init(MB_HW_HandleTypeDef *hw);
MB_ErrorRet MB_HW_Receive(MB_HW_HandleTypeDef *hw);
MB_ErrorRet MB_HW_Send(MB_HW_HandleTypeDef *hw, uint8_t len);
void MB_HW_UART_IRQHandler(UART_HandleTypeDef *huart);

#endif  /* __MB_HW_H__ */
