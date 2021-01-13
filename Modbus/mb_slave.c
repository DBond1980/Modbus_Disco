#include "mb_slave.h"
#include <stdarg.h>
#include <stdlib.h>
#include "mb_rtu.h"
#include "mb_slave_reg.h"
#include <string.h>

typedef struct
{
	void			*Instance;	//Указатель на структуру RTU или в будущем TCP
								//т.е. на структуру представляющую нижний абстрактный слой

	//Callback - функции реализующие интерфейс для связи с нижним абстрактным слоем
	MB_ErrorRet_t	(*SendPDU_Callback)(void *mb,
							 const uint8_t *pdu_buf, uint8_t pdu_len, bool broadcast);	
	void			(*SetSlaveAddress_Callback)(void *mb, uint8_t slave_address);
	uint32_t		DiagnosticSuccessCounter; //Диагностика - счетчик успешно принятых пакетов

} mb_slave_handle_t;

static void mb_slave_receive_event(void *mb_owner, uint8_t *pdu_buf, uint8_t pdu_len, bool broadcast);
static MB_Exception_t mb_slave_func_read_input_registers(uint8_t *pdu_buf, uint8_t *pdu_len);
static MB_Exception_t mb_slave_func_read_holding_registers(uint8_t *pdu_buf, uint8_t *pdu_len);
static MB_Exception_t mb_slave_func_write_holding_register(uint8_t *pdu_buf, uint8_t *pdu_len);
static MB_Exception_t mb_slave_func_write_holding_registers(uint8_t *pdu_buf, uint8_t *pdu_len);
static void mb_slave_error_handler(uint8_t *pdu_buf, uint8_t *pdu_len, MB_Exception_t exception);

static MB_Exception_t mb_slave_read_registers(MB_RegType_t reg_type, uint8_t *data_buf, uint16_t reg_address, int16_t reg_count);
static MB_Exception_t mb_slave_write_holding_registers(uint8_t *data_buf, uint16_t reg_address, uint16_t reg_count);

static MB_SlaveReg_t *mb_slave_regs = NULL; //Указатель на список регистров
static xSemaphoreHandle mb_slave_def_mutex = NULL; //Мьютекс по умолчанию (чтения/записи регистров)

typedef struct
{
	uint8_t FunctionCode;
	MB_Exception_t (*Handler)(uint8_t *pdu_buf, uint8_t *pdu_len);
} MB_Slave_FunctionHandler_t;

static const MB_Slave_FunctionHandler_t mb_slave_func_handlers[] =
	{
		{MB_FUNC_READ_INPUT_REGS,		mb_slave_func_read_input_registers},
		{MB_FUNC_READ_HOLDING_REGS,		mb_slave_func_read_holding_registers},
		{MB_FUNC_WRITE_HOLDING_REG,		mb_slave_func_write_holding_register},
		{MB_FUNC_WRITE_HOLDING_REGS,	mb_slave_func_write_holding_registers},
		
		//{0x02, FuncReadDiscreteInputs},
		//{0x01, FuncReadCoils},
		//{0x05, FuncWriteCoil},
		//{0x0F, FuncWriteMultipleCoils},
		//{0x17, FuncReadWriteMultipleHoldingRegister},
		//{0x11, FuncReportSlaveID},

		{0, NULL}};

//Инициализация (протокол MODBUS RTU) ведомого устройства
//(применяется для нескольких портов)
MB_ErrorRet_t MB_Slave_Init_RTUs(uint8_t slave_address, uint8_t num, ...)
{	
	va_list list;
	va_start(list, num);

	for (uint8_t i = 0; i < num; i++)
	{
		MB_Slave_Init_RTU(slave_address, va_arg(list, UART_HandleTypeDef *));
	}
	
	va_end(list);
	
	return MB_OK;
}

//Инициализация (протокол MODBUS RTU) ведомого устройства (один порт)
MB_ErrorRet_t MB_Slave_Init_RTU(uint8_t slave_address, UART_HandleTypeDef *uart)
{
	//Под структуры MB_RTU_Handle_t и mb_slave_handle_t выделяется память из кучи
	//поэтому инициализацию следует проводить один раз
	//(или предусмотреть очистку free)

	mb_slave_handle_t *mb_slave = malloc(sizeof(mb_slave_handle_t));
	if (mb_slave == NULL) return MB_ERR_MEM;
	
	//Выделение памяти под порт (под структуру MB_RTU_Handle_t)
	MB_RTU_Handle_t *mb_rtu = malloc(sizeof(MB_RTU_Handle_t));
	if (mb_rtu == NULL)	return MB_ERR_MEM;

	//Инициализация структуры MB_RTU_Handle_t начальным состоянием
	MB_RTU_Handle_Default(mb_rtu);

	//Установка инициализация структуры MB_RTU_Handle_t
	mb_rtu->Instance = uart;
	mb_rtu->SlaveAddress = slave_address;
	mb_rtu->Owner = mb_slave;
	//Инициализация порта (модуля MB_RTU)
	MB_ErrorRet_t ret;
	if ((ret = MB_RTU_Init(mb_rtu)) != MB_OK)
		return ret;
	
	//Инициализация callback - функций, которые реализуют связь с
	//нижним абстрактным слоем
	mb_slave->Instance = mb_rtu;
	mb_slave->SendPDU_Callback = MB_RTU_Send_PDU;
	mb_slave->SetSlaveAddress_Callback = MB_RTU_Set_SlaveAddress;
	mb_rtu->ReceiveEvent_Callback = mb_slave_receive_event;
	
	return MB_OK;
}
//Инициализация порта (протокол MODBUS TCP) ведомого устройства
//MB_ErrorRet_t MB_Slave_Init_TCP(uint8_t slave_address, /*______*/)
//{
//}

//Инициализация регистров (регистры общие для всех ведомых устройств)
MB_ErrorRet_t MB_Slave_Init_Registers(MB_SlaveReg_t *regs)
{
	//Создание мьютекса по умолчанию (чтения/записи регистров)
	if (mb_slave_def_mutex == NULL)
		mb_slave_def_mutex = xSemaphoreCreateMutex();

	mb_slave_regs = NULL;
	int i = 0;
	while (regs[i].Data != NULL)
	{
		//Проверка параметров
		if (regs[i].FirstAddr > regs[i].LastAddr) return MB_ERR_ARG;
		if ((regs[i].LastAddr - regs[i].FirstAddr + 1) * 2 < regs[i].DataLen)
			return MB_ERR_ARG;

		//Установка мьютекса по умолчанию если он не задан
		if (regs[i].MutexPtr == NULL)
			regs[i].MutexPtr = &mb_slave_def_mutex;
		
		i++;
	}

	//Установка списка регистров 
	mb_slave_regs = regs;
	
	return MB_OK;
}

//Обработка события (callback - функция) получение пакета модулем MB_RTU
//(для ответа используется тот же буфер pdu_buf, поэтому он должен быть не меньше 256 байт)
static void mb_slave_receive_event(void *mb_owner, uint8_t *pdu_buf, uint8_t pdu_len, bool broadcast)
{
	uint8_t i = 0;
	MB_Exception_t ret;
	mb_slave_handle_t *mb_slave = (mb_slave_handle_t *)mb_owner;

	while (mb_slave_func_handlers[i].FunctionCode != 0)
	{
		if (pdu_buf[MB_PDU_FUNC_OFFSET] == mb_slave_func_handlers[i].FunctionCode) break;
		i++;
	}

	if (mb_slave_func_handlers[i].FunctionCode != 0)
	{
		ret = mb_slave_func_handlers[i].Handler(pdu_buf, &pdu_len);
	}
	else ret = MB_EX_ILLEGAL_FUNCTION;
	
	if (ret != MB_EX_NONE)
		mb_slave_error_handler(pdu_buf, &pdu_len, ret);

	if (!broadcast)
	{
		mb_slave->SendPDU_Callback(mb_slave->Instance, pdu_buf, pdu_len, false);
	}
	mb_slave->DiagnosticSuccessCounter++;
}

//Функция чтения 16-ти битных регистров Inputs
static MB_Exception_t mb_slave_func_read_input_registers(uint8_t *pdu_buf, uint8_t *pdu_len)
{
	//Проверка размера входного пакета
	if (*pdu_len != MB_PDU_READ_DATA_SIZE_MIN + MB_PDU_SIZE_MIN)
		return MB_EX_ILLEGAL_DATA_VALUE;

	//Определение адресса регистров и их количество
	const uint16_t start_reg_addr = pdu_buf[MB_PDU_READ_ADDR_OFFSET] << 8 | pdu_buf[MB_PDU_READ_ADDR_OFFSET + 1];
	const uint16_t reg_count = pdu_buf[MB_PDU_READ_REGCNT_OFFSET] << 8 | pdu_buf[MB_PDU_READ_REGCNT_OFFSET + 1];

	//Проверка количества
	if (reg_count < 1 || reg_count > MB_PDU_READ_REGCNT_MAX)
		return MB_EX_ILLEGAL_DATA_VALUE;

	//Расчет размера выходного пакета
	*pdu_len = MB_PDU_READ_OUT_DATA_OFFSET + reg_count * 2;

	//Расчет количества байт для передачи регистров
	pdu_buf[MB_PDU_READ_OUT_BYTECNT_OFFSET] = reg_count * 2;

	//Чтение регистров
	return mb_slave_read_registers(MB_REG_INPUT, &pdu_buf[MB_PDU_READ_OUT_DATA_OFFSET], start_reg_addr, reg_count);
}

//Функция чтения 16-ти битных регистров Holding
static MB_Exception_t mb_slave_func_read_holding_registers(uint8_t *pdu_buf, uint8_t *pdu_len)
{
	//Проверка размера входного пакета
	if (*pdu_len != MB_PDU_READ_DATA_SIZE_MIN + MB_PDU_SIZE_MIN)
		return MB_EX_ILLEGAL_DATA_VALUE;

	//Определение адресса регистров и их количество
	const uint16_t start_reg_addr = pdu_buf[MB_PDU_READ_ADDR_OFFSET] << 8 | pdu_buf[MB_PDU_READ_ADDR_OFFSET + 1];
	const uint16_t reg_count = pdu_buf[MB_PDU_READ_REGCNT_OFFSET] << 8 | pdu_buf[MB_PDU_READ_REGCNT_OFFSET + 1];

	//Проверка количества
	if (reg_count < 1 || reg_count > MB_PDU_READ_REGCNT_MAX)
		return MB_EX_ILLEGAL_DATA_VALUE;

	//Расчет размера выходного пакета
	*pdu_len = MB_PDU_READ_OUT_DATA_OFFSET + reg_count * 2;

	//Расчет количества байт для передачи регистров
	pdu_buf[MB_PDU_READ_OUT_BYTECNT_OFFSET] = reg_count * 2;

	//Чтение регистров
	return mb_slave_read_registers(MB_REG_HOLDING, &pdu_buf[MB_PDU_READ_OUT_DATA_OFFSET], start_reg_addr, reg_count);
}

//Функция записи одного 16-ти битного регистра Holding
static MB_Exception_t mb_slave_func_write_holding_register(uint8_t *pdu_buf, uint8_t *pdu_len)
{
	//Проверка размера входного пакета
	if (*pdu_len != MB_PDU_WRITE_SIZE + MB_PDU_SIZE_MIN)
		return MB_EX_ILLEGAL_DATA_VALUE;
	
	//Определение адресса регистра
	const uint16_t reg_addr = pdu_buf[MB_PDU_READ_ADDR_OFFSET] << 8 | pdu_buf[MB_PDU_READ_ADDR_OFFSET + 1];
	
	//Запись регистра
	return mb_slave_write_holding_registers(&pdu_buf[MB_PDU_WRITE_MUL_DATA_OFFSET], reg_addr, 1);
}

//Функция записи 16-ти битных регистров Holding
static MB_Exception_t mb_slave_func_write_holding_registers(uint8_t *pdu_buf, uint8_t *pdu_len)
{
	//Проверка минимального размера входного пакета
	if (*pdu_len < MB_PDU_WRITE_MUL_SIZE_MIN + MB_PDU_SIZE_MIN)
		return MB_EX_ILLEGAL_DATA_VALUE;
	
	//Определение адресса регистров, их количество и количество байт с данными
	const uint16_t start_reg_addr = pdu_buf[MB_PDU_WRITE_MUL_ADDR_OFFSET] << 8 | pdu_buf[MB_PDU_WRITE_MUL_ADDR_OFFSET + 1];
	const uint16_t reg_count = pdu_buf[MB_PDU_WRITE_MUL_REGCNT_OFFSET] << 8 | pdu_buf[MB_PDU_WRITE_MUL_REGCNT_OFFSET + 1];
	const uint8_t byte_count = pdu_buf[MB_PDU_WRITE_MUL_BYTECNT_OFFSET];

	//Проверка количества регистров и количество байт с данными 
	if (reg_count < 1 || reg_count > MB_PDU_WRITE_MUL_REGCNT_MAX || 
			byte_count != (uint8_t)(2 * reg_count))
		return MB_EX_ILLEGAL_DATA_VALUE;

	//Размер выходного пакета
	*pdu_len = MB_PDU_WRITE_MUL_BYTECNT_OFFSET;

	//Запись регистров
	return mb_slave_write_holding_registers(&pdu_buf[MB_PDU_WRITE_MUL_DATA_OFFSET], start_reg_addr, reg_count);
}

//Обработчик ошибки (исключения)
static void mb_slave_error_handler(uint8_t *pdu_buf, uint8_t *pdu_len, MB_Exception_t exception)
{
	pdu_buf[MB_PDU_FUNC_OFFSET] |= MB_FUNC_ERROR_MASK;
	pdu_buf[MB_PDU_ERROR_EXCEPTION] = exception;
	*pdu_len = MB_PDU_ERROR_SIZE;
}

//Чтение регистров
static MB_Exception_t mb_slave_read_registers(MB_RegType_t reg_type, uint8_t *data_buf, uint16_t reg_address, int16_t reg_count)
{
	if (mb_slave_regs == NULL)
		return MB_EX_ILLEGAL_DATA_ADDRESS;

	int reg_block = 0;
	while (mb_slave_regs[reg_block].Data != NULL)
	{
		//Поиск подходящего блока регистров ....

		//Проверка типа регистров
		if (mb_slave_regs[reg_block].RegType != reg_type)
		{
			reg_block++;
			continue;
		}

		//Проверка адреса
		if (reg_address >= mb_slave_regs[reg_block].FirstAddr && reg_address <= mb_slave_regs[reg_block].LastAddr)
		{
			//Соответствующий блок регистров найден

			//Захват мьютекса, который соответствует найденному блоку регистров
			if (mb_slave_regs[reg_block].MutexPtr != NULL)
				xSemaphoreTake(*mb_slave_regs[reg_block].MutexPtr, portMAX_DELAY);

			//Расчет начального адреса для чтения данных из регистров
			uint8_t *reg_buf = ((uint8_t *)mb_slave_regs[reg_block].Data) + 
							   (reg_address - mb_slave_regs[reg_block].FirstAddr) * 2;

			while (true)
			{
				if (mb_slave_regs[reg_block].Endian == MB_RF_DATA)
				{
					//Блок данных

					//Расчет количества байт данных для копирования
					int32_t len = mb_slave_regs[reg_block].DataLen - 
								  (reg_buf - (uint8_t *)mb_slave_regs[reg_block].Data);

					if (len == 0)
					{//Количество регистров больше чем массив с данными

						//Расчет количества оставшихся пустых данных в блоке
						len = (mb_slave_regs[reg_block].LastAddr - mb_slave_regs[reg_block].FirstAddr + 1) * 2
								- (reg_buf - (uint8_t *)mb_slave_regs[reg_block].Data);
						
						if (len > reg_count * 2) len = reg_count * 2;
						memset(data_buf, 0, len); //Заполнение нулями
					}
					else
					{
						if (len > reg_count * 2) len = reg_count * 2;
						memcpy(data_buf, reg_buf, len); //Копирование данных
					}

					if (len % 2)
					{ //если не кратно 2
						data_buf[len] = 0;
						len++;
					} 
					data_buf += len;
					reg_buf += len;					
					reg_count -= len / 2; 
					reg_address += len / 2;
				}
				else if (mb_slave_regs[reg_block].Endian == MB_RF_BIG_ENDIAN_32)
				{
					//32-x битные данные в формате big-endian
					
					if (reg_buf + 4 - (uint8_t *)mb_slave_regs[reg_block].Data <= mb_slave_regs[reg_block].DataLen)
					{
						*(data_buf + 0) = *(reg_buf + 3);
						*(data_buf + 1) = *(reg_buf + 2);
						*(data_buf + 2) = *(reg_buf + 1);
						*(data_buf + 3) = *(reg_buf + 0);
					}
					else
					{ //Количество регистров больше чем массив с данными
						*(data_buf + 0) = 0;
						*(data_buf + 1) = 0;
						*(data_buf + 2) = 0;
						*(data_buf + 3) = 0;
					}
					data_buf += 4;
					reg_buf += 4;
					reg_count -= 2;
					reg_address += 2;
				}
				else
				{
					//16-ти битные данные в формате big-endian или little-endian, или
					//32-x битные данные в формате little-endian
					//
					if (reg_buf + 2 - (uint8_t *)mb_slave_regs[reg_block].Data <= mb_slave_regs[reg_block].DataLen)
					{
						if (mb_slave_regs[reg_block].Endian == MB_RF_LITTLE_ENDIAN)
						{
							*(data_buf + 0) = *(reg_buf + 0);
							*(data_buf + 1) = *(reg_buf + 1);
						}
						else // MB_BIG_ENDIAN (MB_BIG_ENDIAN_32)
						{
							*(data_buf + 0) = *(reg_buf + 1);
							*(data_buf + 1) = *(reg_buf + 0);
						}
					}
					else
					{ //Количество регистров больше чем массив с данными
						*(data_buf + 0) = 0;
						*(data_buf + 1) = 0;
					}
					data_buf += 2;
					reg_buf += 2;
					reg_count--;
					reg_address++;
				}

				if (reg_count <= 0 || reg_address > mb_slave_regs[reg_block].LastAddr)
				{
					//Конец

					//Освобождение мьютекса
					if (mb_slave_regs[reg_block].MutexPtr != NULL)
						xSemaphoreGive(*mb_slave_regs[reg_block].MutexPtr);

					//Отдать семафор
					if (mb_slave_regs[reg_block].SemaphorePtr != NULL)
						xSemaphoreGive(*mb_slave_regs[reg_block].SemaphorePtr);

					if (reg_count <= 0)
					{
						//Конец. Все регистры считанны
						return MB_EX_NONE;
					}

					if (reg_address > mb_slave_regs[reg_block].LastAddr)
					{
						//Конец блока регистров.
						//Поиск нового блока регистров начинаем сначала
						//т.к. блоки могут могут иметь произвольную последовательность
						reg_block = -1;
						break;
					}
				}
			}
		}
		reg_block++;
	}

	if (mb_slave_regs[reg_block].Data == NULL)
		return MB_EX_ILLEGAL_DATA_ADDRESS;

	return MB_EX_NONE;
}

//Запись регистров holding
static MB_Exception_t mb_slave_write_holding_registers(uint8_t *data_buf, uint16_t reg_address, uint16_t reg_count)
{
	if (mb_slave_regs == NULL)
		return MB_EX_ILLEGAL_DATA_ADDRESS;

	int reg_block = 0;
	while (mb_slave_regs[reg_block].Data != NULL)
	{
		//Поиск подходящего блока регистров ....

		//Проверка типа регистров
		if (mb_slave_regs[reg_block].RegType != MB_REG_HOLDING)
		{
			reg_block++;
			continue;
		}

		//Проверка адреса
		if (reg_address >= mb_slave_regs[reg_block].FirstAddr && reg_address <= mb_slave_regs[reg_block].LastAddr)
		{
			//Соответствующий блок регистров найден

			//Захват мьютекса, который соответствует найденному блоку регистров
			if (mb_slave_regs[reg_block].MutexPtr != NULL)
				xSemaphoreTake(*mb_slave_regs[reg_block].MutexPtr, portMAX_DELAY); 

			//Расчет начального адреса для записи данных из регистров
			uint8_t *reg_buf = ((uint8_t *)mb_slave_regs[reg_block].Data) + 
							   (reg_address - mb_slave_regs[reg_block].FirstAddr) * 2;

			while (true)
			{
				if (mb_slave_regs[reg_block].Endian == MB_RF_DATA)
				{
					//Блок данных

					//Расчет количества байт данных для копирования
					int32_t len = mb_slave_regs[reg_block].DataLen - 
								  (reg_buf - (uint8_t *)mb_slave_regs[reg_block].Data);

					if (len == 0)
					{ //Количество регистров больше чем массив с данными
						//Расчет количества оставшихся пустых данных в блоке
						len = (mb_slave_regs[reg_block].LastAddr - mb_slave_regs[reg_block].FirstAddr + 1) * 2 -
							  (reg_buf - (uint8_t *)mb_slave_regs[reg_block].Data);
						if (len > reg_count * 2) len = reg_count * 2;
					}
					else
					{
						if (len > reg_count * 2) len = reg_count * 2;
						memcpy(reg_buf, data_buf, len); //Копирование данных
					}
					
					if (len % 2)
					{ //если не кратно 2
						len++;
					}
					data_buf += len;
					reg_buf += len;
					reg_count -= len / 2;
					reg_address += len / 2;
				}
				else if (mb_slave_regs[reg_block].Endian == MB_RF_BIG_ENDIAN_32)
				{
					//32-x битные данные в формате big-endian

					if (reg_buf + 4 - (uint8_t *)mb_slave_regs[reg_block].Data <= mb_slave_regs[reg_block].DataLen)
					{
						*(reg_buf + 3) = *(data_buf + 0);
						*(reg_buf + 2) = *(data_buf + 1);
						*(reg_buf + 1) = *(data_buf + 2);
						*(reg_buf + 0) = *(data_buf + 3);
					}
					data_buf += 4;
					reg_buf += 4;
					reg_count -= 2;
					reg_address += 2;
				}
				else
				{
					//16-ти битные данные в формате big-endian или little-endian, или
					//32-x битные данные в формате little-endian
					
					if (reg_buf + 2 - (uint8_t *)mb_slave_regs[reg_block].Data <= mb_slave_regs[reg_block].DataLen)
					{
						if (mb_slave_regs[reg_block].Endian == MB_RF_LITTLE_ENDIAN)
						{
							*(reg_buf + 0) = *(data_buf + 0);
							*(reg_buf + 1) = *(data_buf + 1);
						}
						else // MB_BIG_ENDIAN (MB_BIG_ENDIAN_32)
						{
							*(reg_buf + 1) = *(data_buf + 0);
							*(reg_buf + 0) = *(data_buf + 1);
						}
					}
					data_buf += 2;
					reg_buf += 2;
					reg_count--;
					reg_address++;
				}

				if (reg_count <= 0 || reg_address > mb_slave_regs[reg_block].LastAddr)
				{
					//Конец
					
					//Освобождение мьютекса
					if (mb_slave_regs[reg_block].MutexPtr != NULL)
						xSemaphoreGive(*mb_slave_regs[reg_block].MutexPtr);

					//Отдать семафор
					if (mb_slave_regs[reg_block].SemaphorePtr != NULL)
						xSemaphoreGive(*mb_slave_regs[reg_block].SemaphorePtr);

					if (reg_count <= 0)
					{
						//Конец. Все регистры записаны
						return MB_EX_NONE;
					}

					if (reg_address > mb_slave_regs[reg_block].LastAddr)
					{
						//Конец блока регистров.
						//Поиск нового блока регистров начинаем сначала
						//т.к. блоки могут могут иметь произвольную последовательность
						reg_block = -1;
						break;
					}
				}
			}
		}
		reg_block++;
	}

	if (mb_slave_regs[reg_block].Data == NULL)
		return MB_EX_ILLEGAL_DATA_ADDRESS;

	return MB_EX_NONE;
}



