#ifndef __MB_RTU_H__
#define __MB_RTU_H__

#include "mb_port.h"
#include "mb.h"
#include "mb_frame.h"

#define MB_RTU_ADU_SIZE_MAX		(MB_ADU_SIZE_MAX + 1) //на один байт больше чем 256, чтобы всегда ловить пакет по тайм-ауту

typedef struct //__MB_RTU_Handle_t
{
	UART_HandleTypeDef	*Instance; //Указатель на структуру UART из HAL
	uint8_t				SlaveAddress;
	uint8_t				ADU_Buf[MB_RTU_ADU_SIZE_MAX];   //Массив для хранения пакета
	uint16_t			ADU_BufLen; //Размер пакета
	void				(*ReceiveEvent_Callback)(void *mb_owner,
								 uint8_t *pdu_buf, uint8_t pdu_len, bool broadcast); //Обработчик события приема пакета								
	bool				InterFrameTimeout_Fix;//Фикстрованый тайм-аут между пакетами InterFrameTimeout
	uint32_t			InterFrameTimeout; //тайм-аут между пакетами в битах, если InterFrameTimeout_Fix = true
	void				*Owner; //Указатель на структуру "родитель" если такая есть 
} MB_RTU_Handle_t;

//Функции для инициализации
void MB_RTU_Handle_Default(MB_RTU_Handle_t *mb_rtu);
MB_ErrorRet_t MB_RTU_Init(MB_RTU_Handle_t *mb_rtu);
void MB_RTU_UART_IRQHandler(UART_HandleTypeDef *huart);

//Функции реализующие интерфейс для связи с следующим абстрактным слоем
MB_ErrorRet_t MB_RTU_Send_PDU(void *mb_rtu, const uint8_t *pdu_buf, uint8_t pdu_len, bool broadcast);
uint8_t *MB_RTU_Get_PDU_Buf(void *mb_rtu);
uint8_t MB_RTU_Get_PDU_Len(void *mb_rtu);
void MB_RTU_Set_SlaveAddress(void *mb_rtu, uint8_t slave_address);

#endif /* __MB_RTU_H__ */
