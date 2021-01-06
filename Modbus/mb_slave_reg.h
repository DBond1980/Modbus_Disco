#ifndef __MB_SLAVE_REG_H__
#define __MB_SLAVE_REG_H__

#include "mb.h"
#include "mb_port.h"

typedef enum
{
	MB_RF_LITTLE_ENDIAN, //16-ти битные регистры little-endian или 32-ти битные регистры little-endian
	MB_RF_BIG_ENDIAN,	 //16-ти битные регистры big-endian
	MB_RF_BIG_ENDIAN_32, //32-ти битные регистры big-endian
	MB_RF_DATA			 //блок данных
} MB_RegFormat;

//Структура хранящая информацию о регистре или блоке регистров
//и используется при инициализации регистров
typedef struct
{
	MB_RegType RegType;   //Тип регистров
	uint16_t FirstAddr; //Адрес первого регистра блока
	uint16_t LastAddr;	//Адрес последнего регистра
	void *Data;			  //Блок данных
	uint16_t DataLen;     //Длина блока данных в байтах
	xSemaphoreHandle *Mutex; //Персональный мьютекс для блока данных
							 //Если NULL -> используется общий мьютекс (по умолчанию)
	xSemaphoreHandle *Semaphore; //Указатель на семафор регулирующий доступ к данным
								 //Если семафор не нужен -> NULL
	MB_RegFormat Endian;
} MB_SlaveReg;


////Структура хранящая информацию о регистре или блоке регистров
////типа Input (только чтение)
//typedef struct 
//{
//	uint16_t FirstAddr; //Адрес первого регистра блока
//	uint16_t LastAddr;	//Адрес последнего регистра
//	void *Data;			//Блок данных
//	uint16_t DataLen;	//Длина блока данных в байтах
//	xSemaphoreHandle *Mutex; //Персональный мьютекс для блока данных
//							 //Если NULL -> используется общий мьютекс
//} MB_Slave_Reg_Input;
//
////Структура хранящая информацию о регистре или блоке регистров
////типа Holding (чтение и запись)
//typedef struct
//{
//	uint16_t FirstAddr;		 //Адрес первого регистра блока
//	uint16_t LastAddr;		 //Адрес последнего регистра
//	void *Data;				 //Блок данных
//	uint16_t DataLen;		 //Длина блока данных в байтах
//	xSemaphoreHandle *Mutex; //Персональный мьютекс для блока данных
//							 //Если NULL -> используется общий мьютекс
//	xSemaphoreHandle *Semaphore;	//Указатель на семафор регулирующий доступ к данным
//									//Если семафор не нужен -> NULL
//} MB_Slave_Reg_Holding;


#endif /* __MB_SLAVE_REG_H__ */

