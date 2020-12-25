#ifndef __MB_HW_H__
#define __MB_HW_H__

#include "stm32f7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "MB.h"

#define MB_HW_ADU_SIZE_MAX      257 //на один байт больше чем 256, чтобы всегда ловить пакет по тайм-ауту
#define MB_HW_ADU_SIZE_MIN      4

//Максимальное количество обслуживаемых портов (UART)
#define MB_HW_PORT_NUM_MAX		5

typedef struct
{
	UART_HandleTypeDef	*uart; //Указатель на структуру UART из HAL
	uint8_t SlaveAddress;
	uint8_t	ADU_Buf[MB_HW_ADU_SIZE_MAX];   //Массив для хранения пакета
	uint16_t BufLen; //Размер пакета
	int(*ReceiveEvent)(uint8_t pdu_buf, uint8_t len);    //Обработчик события приема пакета
	bool InterFrameTimeout_Fix;//Фикстрованый тайм-аут между пакетами InterFrameTimeout
	uint32_t InterFrameTimeout; //тайм-аут между пакетами в битах, если InterFrameTimeout_Fix = true
} MB_RTU_HandleTypeDef;

void MB_RTU_Handle_Default(MB_RTU_HandleTypeDef *hw);
MB_ErrorRet MB_RTU_Init(MB_RTU_HandleTypeDef *hw);
//MB_ErrorRet MB_HW_InitLight(UART_HandleTypeDef *huart);
void MB_RTU_UART_IRQHandler(UART_HandleTypeDef *huart);

#endif  /* __MB_HW_H__ */
