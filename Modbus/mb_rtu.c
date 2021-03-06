#include "mb_rtu.h"
#include "mb_rtu_crc.h"
#include "limits.h"
#include <string.h>

#define MB_RTU_INTER_FRAME_TIMEOUT_3_5_BYTES	35

static MB_RTU_Handle_t *mb_hw_handles[MB_RTU_PORT_NUM_MAX];
static uint8_t mb_hw_handles_num = 0;
static MB_RTU_Handle_t * mb_rtu_get_hw_from_uart(UART_HandleTypeDef *huart);
static void mb_rtu_receive_task(void const *arg);
static osThreadId mb_rtu_receive_task_handle = NULL;
static osMessageQId mb_rtu_receive_queue_handle = NULL;
static MB_ErrorRet_t mb_rtu_start_receive_adu(MB_RTU_Handle_t *mb_rtu);
static MB_ErrorRet_t mb_rtu_start_send_adu(MB_RTU_Handle_t *mb_rtu);
//static MB_ErrorRet_t MB_RTU_Send_PDU(void *mb_rtu, const uint8_t *pdu_buf, uint8_t pdu_len, bool broadcast);

//Инициализация структуры MB_RTU_Handle_t
void MB_RTU_Handle_Default(MB_RTU_Handle_t *mb_rtu)
{
	mb_rtu->Instance = NULL;
	mb_rtu->SlaveAddress = 1;
	mb_rtu->ADU_BufLen = 0;
	mb_rtu->InterFrameTimeout_Fix = true;
	mb_rtu->InterFrameTimeout = MB_RTU_INTER_FRAME_TIMEOUT_3_5_BYTES;
	mb_rtu->ReceiveEvent_Callback = NULL;
	//mb_rtu->SendCallback = MB_RTU_Send_PDU;
	mb_rtu->Owner = NULL; //mb_rtu;
}

//Инициализация аппаратной части
MB_ErrorRet_t MB_RTU_Init(MB_RTU_Handle_t *mb_rtu)
{
	if (mb_rtu == NULL)
	{
		return MB_ERR_ARG;
	}

	if (mb_hw_handles_num >= MB_RTU_PORT_NUM_MAX) return MB_ERR_ARG;
	mb_hw_handles[mb_hw_handles_num++] = mb_rtu;

	if (mb_rtu_receive_queue_handle == NULL)
	{
		//Создание очереди для приема пакетов	
		//mb_rtu_receive_queue_handle = xQueueCreate(MB_HW_PORT_NUM_MAX, sizeof(MB_RTU_Handle_t *));
		osMessageQDef(mb_rtu_receive_queue, MB_RTU_PORT_NUM_MAX, sizeof(MB_RTU_Handle_t *));
		mb_rtu_receive_queue_handle = osMessageCreate(osMessageQ(mb_rtu_receive_queue), NULL);	
	}
	
	if (mb_rtu_receive_task_handle == NULL)
	{
		//Создания задачи обработки принятых пакетов (преобразование ADU -> PDU)
		osThreadDef(mb_rtu_receive_task, mb_rtu_receive_task, osPriorityHigh, 0, 256);
		mb_rtu_receive_task_handle = osThreadCreate(osThread(mb_rtu_receive_task), NULL);		
	}
	
	if (mb_rtu->InterFrameTimeout_Fix)
	{//Для всех скоростей использовать тайм-аут (между пакетами) 3.5 символа (байта)
		mb_rtu->Instance->Instance->RTOR = mb_rtu->InterFrameTimeout;
	}
	else
	{//Использовать тайм-аут рекомендованный в спецификации MODBUS v1.02 стр.13
		if (mb_rtu->Instance->Init.BaudRate >= 19200)
			mb_rtu->InterFrameTimeout = mb_rtu->Instance->Instance->RTOR = 0.00175f * mb_rtu->Instance->Init.BaudRate + 1;
		else
			mb_rtu->InterFrameTimeout = mb_rtu->Instance->Instance->RTOR = MB_RTU_INTER_FRAME_TIMEOUT_3_5_BYTES;
		
	}	
	//Включение прерывания по тайм-ауту между пакетами (HAL не поддерживает)
	SET_BIT(mb_rtu->Instance->Instance->CR1, USART_CR1_RTOIE);

	//Инициировать начало приема
	mb_rtu_start_receive_adu(mb_rtu);
		
	return MB_OK;
}
//void *MB_RTU_Get_Owner(void *mb_rtu)
//{
//	return ((MB_RTU_Handle_t *)mb_rtu)->Owner;
//}
//Установить адрес ведомого устройства
void MB_RTU_Set_SlaveAddress(void *mb_rtu, uint8_t slave_address)
{
	((MB_RTU_Handle_t *)mb_rtu)->SlaveAddress = slave_address;
}
//Получить указатель на PDU пакет
uint8_t *MB_RTU_Get_PDU_Buf(void *mb_rtu)
{
	return &((MB_RTU_Handle_t *)mb_rtu)->ADU_Buf[MB_ADU_PDU_OFFSET];
}
//Получить размер PDU пакета
uint8_t MB_RTU_Get_PDU_Len(void *mb_rtu)
{
	return ((MB_RTU_Handle_t *)mb_rtu)->ADU_BufLen - MB_ADU_PDU_OFFSET - MB_ADU_SIZE_CRC;
}

//Передача пакета PDU
//Преобразование PDU -> ADU
MB_ErrorRet_t MB_RTU_Send_PDU(void *mb_rtu_void, const uint8_t *pdu_buf, uint8_t pdu_len, bool broadcast)
{
	MB_RTU_Handle_t *mb_rtu = mb_rtu_void;
	//Проверка входных данных
	if (pdu_len < 1 || pdu_len > 253 || mb_rtu == NULL || pdu_buf == NULL) 
		return MB_ERR_ARG;

	//Подготовка ADU пакета...
	uint8_t adu_len = 0;

	//Установка адреса
	mb_rtu->ADU_Buf[MB_ADU_ADDR_OFFSET] = broadcast ? 0 : mb_rtu->SlaveAddress;
	adu_len++;

	if (&mb_rtu->ADU_Buf[MB_ADU_PDU_OFFSET] != pdu_buf)
	{
		//Копирование тела пакета
		memcpy(&mb_rtu->ADU_Buf[MB_ADU_PDU_OFFSET], pdu_buf, pdu_len);
	}
	adu_len += pdu_len;

	//Расчет контрольной суммы
	const uint16_t crc = MB_RTU_CRC_Get(mb_rtu->ADU_Buf, adu_len);
	mb_rtu->ADU_Buf[adu_len++] = (uint8_t)(crc & 0xFF);
	mb_rtu->ADU_Buf[adu_len++] = (uint8_t)(crc >> 8);

	//Установка размера пакета
	mb_rtu->ADU_BufLen = adu_len;
	
	return mb_rtu_start_send_adu(mb_rtu);
}

//Задача для обработки принятых пакетов (преобразование ADU -> PDU)
//Обслуживание очереди mb_rtu_receive_queue
//Генерация события о приеме пакета
static void mb_rtu_receive_task(void const *arg)
{
	MB_RTU_Handle_t *mb_rtu;

	while (true)
	{
		//Блокировка задачи на очереди для ожидания поступления данных
		if (xQueueReceive(mb_rtu_receive_queue_handle, &mb_rtu, portMAX_DELAY) != pdPASS) continue;

		bool frame_ok = false;
		
		//Минимальный размер пакета
		if (mb_rtu->ADU_BufLen >= MB_ADU_SIZE_MIN)
		{
			//Проверка адреса
			if (mb_rtu->ADU_Buf[MB_ADU_ADDR_OFFSET] == mb_rtu->SlaveAddress ||
				mb_rtu->ADU_Buf[MB_ADU_ADDR_OFFSET] == MB_BROADCAST_ADDRESS)
			{
				//Проверка CRC
				if (MB_RTU_CRC_Get(mb_rtu->ADU_Buf, mb_rtu->ADU_BufLen) == 0)
				{

					if (mb_rtu->ReceiveEvent_Callback != NULL)
					{
						//Генерация события о приеме пакета
						mb_rtu->ReceiveEvent_Callback(mb_rtu->Owner, &mb_rtu->ADU_Buf[MB_ADU_PDU_OFFSET],
								mb_rtu->ADU_BufLen - MB_ADU_PDU_OFFSET - MB_ADU_SIZE_CRC, 
								mb_rtu->ADU_Buf[MB_ADU_ADDR_OFFSET] == MB_BROADCAST_ADDRESS);
						
						frame_ok = true;
					}
				}
			}
		}
		if (!frame_ok) mb_rtu_start_receive_adu(mb_rtu);
	}
}

//Прием пакета при помощи DMA
static MB_ErrorRet_t mb_rtu_start_receive_adu(MB_RTU_Handle_t *mb_rtu)
{
	if (mb_rtu == NULL) return MB_ERR_ARG;

	mb_rtu->ADU_BufLen = 0;
	SET_BIT(mb_rtu->Instance->Instance->CR2, USART_CR2_RTOEN);
	if (HAL_UART_Receive_DMA(mb_rtu->Instance, mb_rtu->ADU_Buf, MB_RTU_ADU_SIZE_MAX) != HAL_OK)
	{
		return MB_ERR_HAL;
	}
	return MB_OK;
}

//Передача пакетов при помощи DMA
static MB_ErrorRet_t mb_rtu_start_send_adu(MB_RTU_Handle_t *mb_rtu)
{
	if (mb_rtu == NULL) return MB_ERR_ARG;

	if (HAL_UART_Transmit_DMA(mb_rtu->Instance, mb_rtu->ADU_Buf, mb_rtu->ADU_BufLen) != HAL_OK)
	{
		return MB_ERR_HAL;
	}
	return MB_OK;
}

//Вернуть указатель на основную структуру по указателю на порт
static MB_RTU_Handle_t * mb_rtu_get_hw_from_uart(UART_HandleTypeDef *huart)
{
	for (int i = 0; i < mb_hw_handles_num; i++)
	{
		if (mb_hw_handles[i]->Instance == huart) return mb_hw_handles[i];
	}
	return NULL;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{//Прием завершен - HAL Callback (при нормальной работе эта функция не должна вызываться)
	mb_rtu_start_receive_adu(mb_rtu_get_hw_from_uart(huart));
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{//Передача завершена - HAL Callback
	mb_rtu_start_receive_adu(mb_rtu_get_hw_from_uart(huart));	
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{//Ошибка при приеме или передачи - HAL Callback
	
	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//При возникновении апаратной ошибки UART обработчик HAL_UART_IRQHandler
	//выключает DMA, для того чтобы востановить работу UART и DMA после
	//ошибки необходимо установить некоторые биты в регистрах.

	//Сдесь можно реализовать обработку аппаратных ошибок (код в huart->ErrorCode)
	//(parity error, noise error, frame error, overrun error, DMA transfer error)
	//noise error и frame error - могут возникать при тестах на електоромагн. совм.

//	SET_BIT(huart->Instance->CR3, USART_CR3_DMAR);
//	SET_BIT(huart->Instance->CR3, USART_CR3_EIE);
//	__HAL_DMA_ENABLE(huart->hdmarx);

	mb_rtu_start_receive_adu(mb_rtu_get_hw_from_uart(huart));
}

//Нужно добавить вызов этой функции в stm32f7xx_it.c / USART6_IRQHandler
//Обработчик прерывания от UART (работает вместе с обработчиком HAL), должен
//ловить конец пакета при приеме (по тайм-ауту).
//Эта функция (возможность) в HAL не реализована, но аппаратно в микроконтроллере
//предусмотрена и рассчитана на использование с MODBUS.
void MB_RTU_UART_IRQHandler(UART_HandleTypeDef *huart)
{
	if (__HAL_UART_GET_FLAG(huart, UART_FLAG_RTOF))
	{//тайм-аут
		__HAL_UART_CLEAR_IT(huart, UART_CLEAR_RTOF);		
		CLEAR_BIT(huart->Instance->CR2, USART_CR2_RTOEN);
		
		HAL_UART_DMAStop(huart);
		
		MB_RTU_Handle_t *mb_rtu = mb_rtu_get_hw_from_uart(huart);
		if (mb_rtu == NULL) return;

		mb_rtu->ADU_BufLen = MB_RTU_ADU_SIZE_MAX - huart->hdmarx->Instance->NDTR;
		
		if (mb_rtu_receive_task_handle == NULL || mb_rtu_receive_queue_handle == NULL)
		{
			mb_rtu_start_receive_adu(mb_rtu);
			return;
		}

		//Отправка в очередь для дальнейшей обработки
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xQueueSendFromISR(mb_rtu_receive_queue_handle, &mb_rtu, &xHigherPriorityTaskWoken);
		//Переключение контекста выполнения (для того чтобы задача обработчик запустилась сразу после прерывания)
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);		
	}
}

