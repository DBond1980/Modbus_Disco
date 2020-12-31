#include "mb_slave.h"
#include <stdarg.h>
#include <stdlib.h>
#include "mb_rtu.h"

#define MB_PDU_READ_ADDR_OFFSET			(MB_PDU_DATA_OFFSET)
#define MB_PDU_READ_REGCNT_OFFSET		(MB_PDU_DATA_OFFSET + 2)
#define MB_PDU_READ_REGCNT_MAX			(0x007D)
#define MB_PDU_READ_SIZE				(4)
#define MB_PDU_READ_OUT_BYTECNT_OFFSET  (MB_PDU_DATA_OFFSET)
#define MB_PDU_READ_OUT_DATA_OFFSET		(MB_PDU_DATA_OFFSET + 1)

#define MB_PDU_WRITE_MUL_ADDR_OFFSET	(MB_PDU_DATA_OFFSET)
#define MB_PDU_WRITE_MUL_REGCNT_OFFSET	(MB_PDU_DATA_OFFSET + 2)
#define MB_PDU_WRITE_MUL_BYTECNT_OFFSET (MB_PDU_DATA_OFFSET + 4)
#define MB_PDU_WRITE_MUL_VALUES_OFFSET	(MB_PDU_DATA_OFFSET + 5)
#define MB_PDU_WRITE_MUL_SIZE_MIN		(5)
#define MB_PDU_WRITE_MUL_REGCNT_MAX		(0x0078)

#define MB_PDU_WRITE_ADDR_OFFSET		(MB_PDU_DATA_OFFSET)
#define MB_PDU_WRITE_VALUE_OFFSET		(MB_PDU_DATA_OFFSET + 2)
#define MB_PDU_WRITE_SIZE				(4)

#define MB_PDU_ERROR_EXCEPTION			(MB_PDU_DATA_OFFSET)
#define MB_PDU_ERROR_SIZE				(2)

static void mb_slave_receive_event(MB_RTU_HandleTypeDef *mb_rtu, uint8_t *pdu_buf, uint8_t pdu_len, bool broadcast);
static MB_Exception mb_slave_func_read_input_registers(uint8_t *pdu_buf, uint8_t *pdu_len);
static MB_Exception mb_slave_func_read_holding_registers(uint8_t *pdu_buf, uint8_t *pdu_len);
static MB_Exception mb_slave_func_write_holding_register(uint8_t *pdu_buf, uint8_t *pdu_len);
static MB_Exception mb_slave_func_write_holding_registers(uint8_t *pdu_buf, uint8_t *pdu_len);
static void mb_slave_error_handler(uint8_t *pdu_buf, uint8_t *pdu_len, MB_Exception exception);

static MB_Exception mb_slave_read_input_registers(uint8_t *data_buf, uint16_t reg_address, uint16_t reg_count);
static MB_Exception mb_slave_read_holding_registers(uint8_t *data_buf, uint16_t reg_address, uint16_t reg_count);
static MB_Exception mb_slave_write_holding_registers(uint8_t *data_buf, uint16_t reg_address, uint16_t reg_count);

typedef struct
{
	uint8_t FunctionCode;
	MB_Exception (*Handler)(uint8_t *pdu_buf, uint8_t *pdu_len);
} MB_Slave_FunctionHandler;

static const MB_Slave_FunctionHandler mb_slave_func_handlers[] =
	{
		{0x04, mb_slave_func_read_input_registers},
		{0x03, mb_slave_func_read_holding_registers},
		{0x06, mb_slave_func_write_holding_register},
		{0x10, mb_slave_func_write_holding_registers},
		
		//{0x02, FuncReadDiscreteInputs},
		//{0x01, FuncReadCoils},
		//{0x05, FuncWriteCoil},
		//{0x0F, FuncWriteMultipleCoils},
		//{0x17, FuncReadWriteMultipleHoldingRegister},
		//{0x11, FuncReportSlaveID},

		{0, NULL}};

//Инициализация портов (модулей MB_RTU) ведомого устройства
MB_ErrorRet MB_Slave_Init(uint8_t address, uint8_t num, ...)
{	
	//Под структуры MB_RTU_HandleTypeDef выделяется память из кучи
	//поэтому инициализацию следует проводить один раз
	//(или предусмотреть очистку free)
	
	va_list list;
	va_start(list, num);

	for (uint8_t i = 0; i < num; i++)
	{
		//Выделение памяти под порт (под структуру MB_RTU_HandleTypeDef)
		MB_RTU_HandleTypeDef *mb_rtu = malloc(sizeof(MB_RTU_HandleTypeDef));
		if (mb_rtu == NULL)	return MB_ERR_MEM;

		//Инициализация структуры MB_RTU_HandleTypeDef начальным состоянием
		MB_RTU_Handle_Default(mb_rtu);

		//Установка инициализация структуры MB_RTU_HandleTypeDef
		mb_rtu->Instance = va_arg(list, UART_HandleTypeDef *);
		mb_rtu->SlaveAddress = address;
		mb_rtu->ReceiveEventCallback = mb_slave_receive_event;

		//Инициализация порта (модуля MB_RTU)
		MB_ErrorRet ret;
		if ((ret = MB_RTU_Init(mb_rtu)) != MB_OK)
			return ret;
	}
	
	va_end(list);
	
	return MB_OK;
}

//Обработка события (callback - функция) получение пакета модулем MB_RTU
static void mb_slave_receive_event(MB_RTU_HandleTypeDef *mb_rtu, uint8_t *pdu_buf, uint8_t pdu_len, bool broadcast)
{
	uint8_t i = 0;
	MB_Exception ret;

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
		mb_slave_error_handler(pdu_buf, &pdu_len, MB_EX_ILLEGAL_FUNCTION);

	if (!broadcast)
		mb_rtu->SendCallback(mb_rtu, pdu_buf, pdu_len, false);
}

//Функция чтения 16-ти битных регистров Inputs
static MB_Exception mb_slave_func_read_input_registers(uint8_t *pdu_buf, uint8_t *pdu_len)
{
	//Проверка размера входного пакета
	if (*pdu_len != MB_PDU_READ_SIZE + MB_PDU_SIZE_MIN)
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
	return mb_slave_read_input_registers(&pdu_buf[MB_PDU_READ_OUT_DATA_OFFSET], start_reg_addr, reg_count);
}

//Функция чтения 16-ти битных регистров Holding
static MB_Exception mb_slave_func_read_holding_registers(uint8_t *pdu_buf, uint8_t *pdu_len)
{
	//Проверка размера входного пакета
	if (*pdu_len != MB_PDU_READ_SIZE + MB_PDU_SIZE_MIN)
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
	return mb_slave_read_holding_registers(&pdu_buf[MB_PDU_READ_OUT_DATA_OFFSET], start_reg_addr, reg_count);
}

//Функция записи одного 16-ти битного регистра Holding
static MB_Exception mb_slave_func_write_holding_register(uint8_t *pdu_buf, uint8_t *pdu_len)
{
	//Проверка размера входного пакета
	if (*pdu_len != MB_PDU_WRITE_SIZE + MB_PDU_SIZE_MIN)
		return MB_EX_ILLEGAL_DATA_VALUE;
	
	//Определение адресса регистра
	const uint16_t reg_addr = pdu_buf[MB_PDU_READ_ADDR_OFFSET] << 8 | pdu_buf[MB_PDU_READ_ADDR_OFFSET + 1];
	
	//Запись регистра
	return mb_slave_write_holding_registers(&pdu_buf[MB_PDU_WRITE_MUL_VALUES_OFFSET], reg_addr, 1);
}

//Функция записи 16-ти битных регистров Holding
static MB_Exception mb_slave_func_write_holding_registers(uint8_t *pdu_buf, uint8_t *pdu_len)
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
	return mb_slave_write_holding_registers(&pdu_buf[MB_PDU_WRITE_MUL_VALUES_OFFSET], start_reg_addr, reg_count);
}

//Обработчик ошибки (исключения)
static void mb_slave_error_handler(uint8_t *pdu_buf, uint8_t *pdu_len, MB_Exception exception)
{
	pdu_buf[MB_PDU_FUNC_OFFSET] |= MB_FUNC_ERROR_MASK;
	pdu_buf[MB_PDU_ERROR_EXCEPTION] = exception;
	*pdu_len = MB_PDU_ERROR_SIZE;
}

static MB_Exception mb_slave_read_input_registers(uint8_t *data_buf, uint16_t reg_address, uint16_t reg_count)
{
	static uint16_t a = 0;
	
	for (uint16_t i = 0; i < reg_count; i++)
	{
		*((uint16_t *)data_buf) = __REV16(i+a);
		data_buf += 2;
	}
	a++;
	return MB_EX_NONE;
}

static MB_Exception mb_slave_read_holding_registers(uint8_t *data_buf, uint16_t reg_address, uint16_t reg_count)
{
	static uint16_t a = 0;
	
	for (uint16_t i = 0; i < reg_count; i++)
	{
		*((uint16_t *)data_buf) = __REV16(i+a);
		data_buf += 2;
		
	}
	a++;
	return MB_EX_NONE;
}

static MB_Exception mb_slave_write_holding_registers(uint8_t *data_buf, uint16_t reg_address, uint16_t reg_count)
{

	return MB_EX_NONE;
}



