#ifndef __MB_MASTER_H__
#define __MB_MASTER_H__

#include "mb.h"
#include "mb_port.h"
#include "mb_rtu.h"

typedef struct 
{
	void				*Instance;	//Указатель на структуру RTU или в будущем TCP
									//т.е. на структуру представляющую нижний абстрактный слой

	//Callback - функции реализующие интерфейс для связи с нижним абстрактным слоем
	MB_ErrorRet_t		(*SendPDU_Callback)(void *mb, 
							const uint8_t *pdu_buf, uint8_t pdu_len, bool broadcast);
	uint8_t *			(*GetPDUBuf_Callback)(void *mb);
	uint8_t				(*GetPDULen_Callback)(void *mb);
	void				(*SetSlaveAddress_Callback)(void *mb, uint8_t slave_address);
	
	uint16_t			ReceiveTimeOut;	//Тайм-аут в миллисекундах
	uint16_t			RepeatNumber;		//Количество повторов при передачи/приеме пакетов
	xSemaphoreHandle	Mutex;
	xSemaphoreHandle	Semaphore;
	uint32_t			DiagnosticRepeatCounter;  //Диагностика - счетчик повторов (сбоев при передачи/приеме пакетов)
	uint32_t			DiagnosticSuccessCounter; //Диагностика - счетчик успешно переданных пакетов
} MB_Master_Handle_t;

MB_ErrorRet_t MB_Master_Init_RTU(MB_Master_Handle_t *mb_master, UART_HandleTypeDef *uart,
								 uint16_t time_out, uint16_t repeat_number);

MB_Exception_t MB_Master_Read_Registers(MB_Master_Handle_t *mb_master, uint8_t slave_address,
										MB_RegType_t reg_type, uint16_t reg_address,
										void *data_buf, uint16_t size_in_bytes, MB_RegFormat_t reg_format);

MB_Exception_t MB_Master_Write_Registers(MB_Master_Handle_t *mb_master, uint8_t slave_address,
										 MB_RegType_t reg_type, uint16_t reg_address,
										 void *data_buf, uint16_t size_in_bytes, MB_RegFormat_t reg_format,
										 bool broadcast);

//MB_ErrorRet_t MB_Master_Init_TCP(MB_Master_Handle_t *mb_master, /*______*/,
//								 uint16_t receive_time_out, uint16_t repeat_number)
								 

#endif /* __MB_MASTER_H__ */

