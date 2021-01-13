#include "mb_master.h"
#include <stdlib.h>
#include <string.h>

static void mb_master_receive_event(MB_RTU_Handle_t *mb_rtu, uint8_t *pdu_buf, uint8_t pdu_len, bool broadcast);
static MB_Exception_t mb_master_send_receive(MB_Master_Handle_t *mb_master, uint8_t *pdu_len, bool broadcast);

//Инициализация ведущего устройства
MB_ErrorRet_t MB_Master_Init_RTU(MB_Master_Handle_t *mb_master, UART_HandleTypeDef *uart, 
								 uint16_t receive_time_out,  uint16_t repeat_number)
{
	//Под структуры MB_RTU_Handle_t выделяется память из кучи
	//поэтому инициализацию следует проводить один раз
	//(или предусмотреть очистку free)

	//Выделение памяти под порт (под структуру MB_RTU_Handle_t)
	MB_RTU_Handle_t *mb_rtu = malloc(sizeof(MB_RTU_Handle_t));
	if (mb_rtu == NULL) return MB_ERR_MEM;

	//Инициализация структуры MB_RTU_Handle_t начальным состоянием
	MB_RTU_Handle_Default(mb_rtu);

	//Установка инициализация структуры MB_RTU_Handle_t
	mb_rtu->Instance = uart;
	mb_rtu->SlaveAddress = 1;
	mb_rtu->ReceiveEventCallback = mb_master_receive_event;
	mb_rtu->Owner = mb_master;

	//Инициализация порта (модуля MB_RTU)
	MB_ErrorRet_t ret;
	if ((ret = MB_RTU_Init(mb_rtu)) != MB_OK)
		return ret;

	//Инициализация структуры MB_Master_Handle_t
	mb_master->Instance = mb_rtu;
	mb_master->ReceiveTimeOut = receive_time_out;
	mb_master->RepeatNumber = repeat_number;
	mb_master->Mutex = xSemaphoreCreateMutex(); 
	mb_master->Semaphore = xSemaphoreCreateBinary();
	mb_master->DiagnosticRepeatCounter = 0;
	mb_master->DiagnosticSuccessCounter = 0;

	return MB_OK;
}

//Чтение регистров ведомого устройства
//size_in_bytes - должен быть кратным двум (иначе последний байт не будет считываться)
MB_Exception_t MB_Master_Read_Registers(MB_Master_Handle_t *mb_master, uint8_t slave_address, 
									  MB_RegType_t reg_type, uint16_t reg_address, 
									  void *data_buf, uint16_t size_in_bytes, MB_RegFormat_t reg_format)
{
	uint8_t *buf = (uint8_t *)data_buf;
	uint8_t *pdu_buf = MB_RTU_Get_PDU(mb_master->Instance);
	
	//Захват мьютекса
	xSemaphoreTake(mb_master->Mutex, portMAX_DELAY);
	
	mb_master->Instance->SlaveAddress = slave_address;

	pdu_buf[MB_PDU_FUNC_OFFSET] = reg_type == MB_REG_INPUT ? 
				MB_FUNC_READ_INPUT_REGS : MB_FUNC_READ_HOLDING_REGS;

	uint16_t reg_count = size_in_bytes / 2;
	
	while (reg_count > 0)
	{
		uint16_t pdu_reg_count = reg_count > MB_PDU_READ_REGCNT_MAX ? MB_PDU_READ_REGCNT_MAX : reg_count;

		if (reg_format == MB_RF_BIG_ENDIAN_32)
			pdu_reg_count &= 0xFFFE; //Делаем кратно двум регистрам (кратно 4-м байтам)

		pdu_buf[MB_PDU_READ_ADDR_OFFSET] = (uint8_t)((reg_address >> 8) & 0xFF);
		pdu_buf[MB_PDU_READ_ADDR_OFFSET + 1] = (uint8_t)(reg_address & 0xFF);

		pdu_buf[MB_PDU_READ_REGCNT_OFFSET] = (uint8_t)((pdu_reg_count >> 8) & 0xFF);
		pdu_buf[MB_PDU_READ_REGCNT_OFFSET + 1] = (uint8_t)(pdu_reg_count & 0xFF);

		uint8_t pdu_len = MB_PDU_READ_DATA_SIZE_MIN + MB_PDU_SIZE_MIN;

		MB_Exception_t ret = mb_master_send_receive(mb_master, &pdu_len, false);
		if (ret != MB_EX_NONE)
		{//Ошибка
			//Освобождение мьютекса
			xSemaphoreGive(mb_master->Mutex);
			return ret;	
		}
		//Проверка размера полученного пакета
		if (pdu_len != MB_PDU_READ_OUT_DATA_SIZE_MIN + MB_PDU_SIZE_MIN +
							pdu_buf[MB_PDU_READ_OUT_BYTECNT_OFFSET])
			return MB_EX_ILLEGAL_DATA_VALUE;

		if (reg_format == MB_RF_DATA || reg_format == MB_RF_LITTLE_ENDIAN)
		{
			//Блок данных
			//16-x битные данные в формате little-endian
			//32-x битные данные в формате little-endian
			memcpy(buf, &pdu_buf[MB_PDU_READ_OUT_DATA_OFFSET], pdu_reg_count * 2);
		}
		else if (reg_format == MB_RF_BIG_ENDIAN)
		{
			//16-x битные данные в формате big-endian
			for (int i = 0; i < pdu_reg_count * 2; i+=2)
			{
				*(buf + i + 0) = *(pdu_buf + MB_PDU_READ_OUT_DATA_OFFSET + i + 1);
				*(buf + i + 1) = *(pdu_buf + MB_PDU_READ_OUT_DATA_OFFSET + i + 0);
			}
		}
		else //if (reg_format == MB_RF_BIG_ENDIAN_32)
		{
			//32-x битные данные в формате big-endian
			for (int i = 0; i < pdu_reg_count * 2; i += 4)
			{
				*(buf + i + 0) = *(pdu_buf + MB_PDU_READ_OUT_DATA_OFFSET + i + 3);
				*(buf + i + 1) = *(pdu_buf + MB_PDU_READ_OUT_DATA_OFFSET + i + 2);
				*(buf + i + 2) = *(pdu_buf + MB_PDU_READ_OUT_DATA_OFFSET + i + 1);
				*(buf + i + 3) = *(pdu_buf + MB_PDU_READ_OUT_DATA_OFFSET + i + 0);
			}
		}
		
		buf += pdu_reg_count * 2;
		reg_count -= pdu_reg_count;
		reg_address += pdu_reg_count;
	}

	//Освобождение мьютекса
	xSemaphoreGive(mb_master->Mutex);
	
	return MB_EX_NONE;
}

//Запись регистров ведомого устройства
MB_Exception_t MB_Master_Write_Registers(MB_Master_Handle_t *mb_master, uint8_t slave_address,
										 MB_RegType_t reg_type, uint16_t reg_address,
										 void *data_buf, uint16_t size_in_bytes, MB_RegFormat_t reg_format,
										 bool broadcast)
{
	uint8_t *buf = (uint8_t *)data_buf;
	uint8_t *pdu_buf = MB_RTU_Get_PDU(mb_master->Instance);

	if (reg_type != MB_REG_HOLDING) return MB_EX_ILLEGAL_FUNCTION;

	//Захват мьютекса
	xSemaphoreTake(mb_master->Mutex, portMAX_DELAY);

	mb_master->Instance->SlaveAddress = slave_address;

	pdu_buf[MB_PDU_FUNC_OFFSET] = MB_FUNC_WRITE_HOLDING_REGS;

	uint16_t reg_count = size_in_bytes / 2;

	while (reg_count > 0)
	{
		uint16_t pdu_reg_count = reg_count > MB_PDU_WRITE_MUL_REGCNT_MAX ?
						MB_PDU_WRITE_MUL_REGCNT_MAX : reg_count;

		if (reg_format == MB_RF_BIG_ENDIAN_32)
			pdu_reg_count &= 0xFFFE; //Делаем кратно двум регистрам (кратно 4-м байтам)

		pdu_buf[MB_PDU_WRITE_MUL_ADDR_OFFSET] = (uint8_t)((reg_address >> 8) & 0xFF);
		pdu_buf[MB_PDU_WRITE_MUL_ADDR_OFFSET + 1] = (uint8_t)(reg_address & 0xFF);

		pdu_buf[MB_PDU_WRITE_MUL_REGCNT_OFFSET] = (uint8_t)((pdu_reg_count >> 8) & 0xFF);
		pdu_buf[MB_PDU_WRITE_MUL_REGCNT_OFFSET + 1] = (uint8_t)(pdu_reg_count & 0xFF);

		pdu_buf[MB_PDU_WRITE_MUL_BYTECNT_OFFSET] = pdu_reg_count * 2;

		uint8_t pdu_len = MB_PDU_WRITE_MUL_SIZE_MIN + MB_PDU_SIZE_MIN +
						  pdu_buf[MB_PDU_WRITE_MUL_BYTECNT_OFFSET];

		if (reg_format == MB_RF_DATA || reg_format == MB_RF_LITTLE_ENDIAN)
		{
			//Блок данных
			//16-x битные данные в формате little-endian
			//32-x битные данные в формате little-endian
			memcpy(&pdu_buf[MB_PDU_WRITE_MUL_DATA_OFFSET], buf, pdu_reg_count * 2);
		}
		else if (reg_format == MB_RF_BIG_ENDIAN)
		{
			//16-x битные данные в формате big-endian
			for (int i = 0; i < pdu_reg_count * 2; i += 2)
			{
				*(pdu_buf + MB_PDU_WRITE_MUL_DATA_OFFSET + i + 1) = *(buf + i + 0);
				*(pdu_buf + MB_PDU_WRITE_MUL_DATA_OFFSET + i + 0) = *(buf + i + 1);
			}
		}
		else //if (reg_format == MB_RF_BIG_ENDIAN_32)
		{
			//32-x битные данные в формате big-endian
			for (int i = 0; i < pdu_reg_count * 2; i += 4)
			{
				*(pdu_buf + MB_PDU_WRITE_MUL_DATA_OFFSET + i + 3) = *(buf + i + 0);
				*(pdu_buf + MB_PDU_WRITE_MUL_DATA_OFFSET + i + 2) = *(buf + i + 1);
				*(pdu_buf + MB_PDU_WRITE_MUL_DATA_OFFSET + i + 1) = *(buf + i + 2);
				*(pdu_buf + MB_PDU_WRITE_MUL_DATA_OFFSET + i + 0) = *(buf + i + 3);
			}
		}

		MB_Exception_t ret = mb_master_send_receive(mb_master, &pdu_len, broadcast);
		if (ret != MB_EX_NONE)
		{ //Ошибка
			//Освобождение мьютекса
			xSemaphoreGive(mb_master->Mutex);
			return ret;
		}

		if (!broadcast)
		{
			//Проверка размера полученного пакета
			if (pdu_len != MB_PDU_WRITE_MUL_OUT_SIZE_MIN + MB_PDU_SIZE_MIN)
				return MB_EX_ILLEGAL_DATA_VALUE;
			//Проверка адреса
			if (reg_address != (pdu_buf[MB_PDU_WRITE_MUL_ADDR_OFFSET] << 8 | pdu_buf[MB_PDU_WRITE_MUL_ADDR_OFFSET + 1]))
				return MB_EX_ILLEGAL_DATA_VALUE;
			//Проверка адреса количества регистров
			if (pdu_reg_count != (pdu_buf[MB_PDU_WRITE_MUL_REGCNT_OFFSET] << 8 | pdu_buf[MB_PDU_WRITE_MUL_REGCNT_OFFSET + 1]))
				return MB_EX_ILLEGAL_DATA_VALUE;
		}
		
		buf += pdu_reg_count * 2;
		reg_count -= pdu_reg_count;
		reg_address += pdu_reg_count;
	}

	//Освобождение мьютекса
	xSemaphoreGive(mb_master->Mutex);

	return MB_EX_NONE;
}

//Передача и прием посылки с повторами
static MB_Exception_t mb_master_send_receive(MB_Master_Handle_t *mb_master, uint8_t *pdu_len, bool broadcast)
{
	uint8_t *pdu_buf = MB_RTU_Get_PDU(mb_master->Instance);

	for (int i = 0; i < mb_master->RepeatNumber; i++)
	{
		if (mb_master->Instance->SendCallback(mb_master->Instance,
				pdu_buf, *pdu_len, broadcast) != MB_OK) return MB_EX_SEND_ERROR;

		if (broadcast) return MB_EX_NONE;

		if (xSemaphoreTake(mb_master->Semaphore, mb_master->ReceiveTimeOut) == pdTRUE)
		{
			if (pdu_buf[MB_PDU_FUNC_OFFSET] & MB_FUNC_ERROR_MASK)
			{//Ведомое устройство вернуло ошибку
				const MB_Exception_t ex = (MB_Exception_t)pdu_buf[MB_PDU_ERROR_EXCEPTION];
				if (ex == MB_EX_ILLEGAL_FUNCTION ||
					ex == MB_EX_ILLEGAL_DATA_ADDRESS ||
					ex == MB_EX_ILLEGAL_DATA_VALUE)
				{
					return ex; //Вернуть ошибку
				}
				if (i + 1 == mb_master->RepeatNumber)
				{	//Последний повтор
					return ex; //Вернуть ошибку
				}
				continue; //Повтор посылки
			}
			//Расчет размера пакета PDU
			*pdu_len = MB_RTU_Get_PDU_Len(mb_master->Instance);
			mb_master->DiagnosticSuccessCounter++; 
			return MB_EX_NONE; //Ответ принят успешно
		}
		mb_master->DiagnosticRepeatCounter++;
	}
	return MB_EX_RECEIVE_ERROR;
}

//Обработка события (callback - функция) получение пакета модулем MB_RTU
//(для ответа используется тот же буфер pdu_buf, поэтому он должен быть не меньше 256 байт)
static void mb_master_receive_event(MB_RTU_Handle_t *mb_rtu, uint8_t *pdu_buf, uint8_t pdu_len, bool broadcast)
{
	//Установка семафора для вывода задачи, которая ждет ответ (функция mb_master_send_receive),
	//из блокированного состояния
	xSemaphoreGive(((MB_Master_Handle_t *)mb_rtu->Owner)->Semaphore);
}