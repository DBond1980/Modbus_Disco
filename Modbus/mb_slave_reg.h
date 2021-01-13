#ifndef __MB_SLAVE_REG_H__
#define __MB_SLAVE_REG_H__

#include "mb.h"
#include "mb_port.h"

//Структура хранящая информацию о регистре или блоке регистров
//и используется при инициализации регистров
typedef struct
{
	MB_RegType_t RegType;		//Тип регистров
	uint16_t FirstAddr;			//Адрес первого регистра блока
	uint16_t LastAddr;			//Адрес последнего регистра
	void *Data;					//Блок данных
	uint16_t DataLen;			//Длина блока данных в байтах
	xSemaphoreHandle *MutexPtr; //Персональный мьютекс для блока данных
								//Если NULL -> используется общий мьютекс (по умолчанию)
	xSemaphoreHandle *SemaphorePtr; //Указатель на семафор регулирующий доступ к данным
									//Если семафор не нужен -> NULL
	MB_RegFormat_t Endian;
} MB_SlaveReg_t;

#endif /* __MB_SLAVE_REG_H__ */

