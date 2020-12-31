#ifndef __MB_RTU_H__
#define __MB_RTU_H__

#include "mb_port.h"
#include "mb.h"
#include "mb_frame.h"

#define MB_RTU_ADU_SIZE_MAX		(MB_ADU_SIZE_MAX + 1) //на один байт больше чем 256, чтобы всегда ловить пакет по тайм-ауту

typedef struct __MB_RTU_HandleTypeDef
{
	UART_HandleTypeDef	*Instance; //Указатель на структуру UART из HAL
	uint8_t				SlaveAddress;
	uint8_t				ADU_Buf[MB_RTU_ADU_SIZE_MAX];   //Массив для хранения пакета
	uint16_t			BufLen; //Размер пакета
	void				(*ReceiveEventCallback)(struct __MB_RTU_HandleTypeDef *mb_rtu,
								 uint8_t *pdu_buf, uint8_t pdu_len, bool broadcast);     //Обработчик события приема пакета
	MB_ErrorRet			(*SendCallback)(struct __MB_RTU_HandleTypeDef *mb_rtu, 
								const uint8_t *pdu_buf, uint8_t pdu_len, bool broadcast);
	bool				InterFrameTimeout_Fix;//Фикстрованый тайм-аут между пакетами InterFrameTimeout
	uint32_t			InterFrameTimeout; //тайм-аут между пакетами в битах, если InterFrameTimeout_Fix = true
} MB_RTU_HandleTypeDef;

void MB_RTU_Handle_Default(MB_RTU_HandleTypeDef *mb_rtu);
MB_ErrorRet MB_RTU_Init(MB_RTU_HandleTypeDef *mb_rtu);
void MB_RTU_UART_IRQHandler(UART_HandleTypeDef *huart);

#endif /* __MB_RTU_H__ */
