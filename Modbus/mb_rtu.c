#include "mb_rtu.h"
#include "limits.h"

static MB_RTU_HandleTypeDef *mb_hw_handles[MB_HW_PORT_NUM_MAX];
static uint8_t mb_hw_handles_num = 0;
static MB_RTU_HandleTypeDef * mb_rtu_get_hw_from_uart(UART_HandleTypeDef *huart);
static void mb_rtu_receive_task(void const *arg);
static void mb_rtu_send_task(void const *arg);
static osThreadId mb_rtu_receive_task_handle = NULL;
static osThreadId mb_rtu_send_task_handle = NULL;
static osMessageQId mb_rtu_receive_queue_handle = NULL;
static osMessageQId mb_rtu_send_queue_handle = NULL;
static MB_ErrorRet mb_rtu_receive_adu(MB_RTU_HandleTypeDef *hw);
static MB_ErrorRet mb_rtu_send_adu(MB_RTU_HandleTypeDef *hw, uint8_t len);

//Инициализация структуры MB_RTU_HandleTypeDef
void MB_RTU_Handle_Default(MB_RTU_HandleTypeDef *hw)
{
	hw->uart = NULL;
	hw->SlaveAddress = 1;
	hw->BufLen = 0;
	hw->ReceiveEvent = NULL;
	hw->InterFrameTimeout_Fix = true;
	hw->InterFrameTimeout = 35;
}

////Упрощенная инициализация аппаратной части
//MB_ErrorRet MB_HW_InitLight(UART_HandleTypeDef *huart)
//{
//	
//	MB_RTU_Handle_Default()
//}

//Инициализация аппаратной части
MB_ErrorRet MB_RTU_Init(MB_RTU_HandleTypeDef *hw)
{
	if (hw == NULL)
	{
		return MB_ERR_ARG;
	}

	if (mb_hw_handles_num >= MB_HW_PORT_NUM_MAX) return MB_ERR_ARG;
	mb_hw_handles[mb_hw_handles_num++] = hw;

	if (mb_rtu_receive_queue_handle == NULL)
	{
		//Создание очереди для приема пакетов	
		//mb_rtu_receive_queue_handle = xQueueCreate(MB_HW_PORT_NUM_MAX, sizeof(MB_RTU_HandleTypeDef *));
		osMessageQDef(mb_rtu_receive_queue, MB_HW_PORT_NUM_MAX, sizeof(MB_RTU_HandleTypeDef *));
		mb_rtu_receive_queue_handle = osMessageCreate(osMessageQ(mb_rtu_receive_queue), NULL);	
	}
	if (mb_rtu_send_queue_handle == NULL)
	{
		//Создание очереди для передачи пакетов	
		osMessageQDef(mb_rtu_send_queue, MB_HW_PORT_NUM_MAX, sizeof(MB_RTU_HandleTypeDef *));
		mb_rtu_send_queue_handle = osMessageCreate(osMessageQ(mb_rtu_send_queue), NULL);	
	}
	if (mb_rtu_receive_task_handle == NULL)
	{
		//Создания задачи обработки принятых пакетов (преобразование ADU -> PDU)
		osThreadDef(mb_rtu_receive_task, mb_rtu_receive_task, osPriorityNormal, 0, 256);
		mb_rtu_receive_task_handle = osThreadCreate(osThread(mb_rtu_receive_task), NULL);		
	}
	if (mb_rtu_send_task_handle == NULL)
	{
		//Создания задачи для передачи пакетов (преобразование PDU -> ADU)
		osThreadDef(mb_rtu_send_task, mb_rtu_send_task, osPriorityNormal, 0, 256);
		mb_rtu_send_task_handle = osThreadCreate(osThread(mb_rtu_send_task), NULL);		
	}

	
	if (hw->InterFrameTimeout_Fix)
	{//Для всех скоростей использовать тайм-аут (между пакетами) 3.5 символа (байта)
		hw->uart->Instance->RTOR = hw->InterFrameTimeout;
	}
	else
	{//Использовать тайм-аут рекомендованный в спецификации MODBUS v1.02 стр.13
		if (hw->uart->Init.BaudRate >= 19200)
			hw->InterFrameTimeout = hw->uart->Instance->RTOR = 0.00175f * hw->uart->Init.BaudRate + 1;
		else
			hw->InterFrameTimeout = hw->uart->Instance->RTOR = 35;
		
	}	
	//Включение прерывания по тайм-ауту между пакетами (HAL не поддерживает)
	SET_BIT(hw->uart->Instance->CR1, USART_CR1_RTOIE);

	
		
	return MB_OK;
}

//Передача пакета PDU
//Преобразование PDU -> ADU
//Установка в очередь передачи
MB_ErrorRet MB_RTU_Send_PDU(MB_RTU_HandleTypeDef *hw, u_int8_t pdu_buf, uint8_t len)
{

	
	return MB_OK;
}

//Задача для передачи пакетов
//Обслуживание очереди mb_rtu_send_queue
static void mb_rtu_send_task(void const *arg)
{
	MB_RTU_HandleTypeDef *hw;
	
	while (true)
	{
		//Блокировка задачи на очереди для ожидания поступления данных
		if(xQueueReceive(mb_rtu_send_queue_handle, &hw, portMAX_DELAY) != pdPASS) continue;



		osDelay(1);
	}
}

//Задача для обработки принятых пакетов (преобразование ADU -> PDU)
//Обслуживание очереди mb_rtu_receive_queue
//Генерация события о приеме пакета
static void mb_rtu_receive_task(void const *arg)
{
	MB_RTU_HandleTypeDef *hw;

	while (true)
	{
		//Блокировка задачи на очереди для ожидания поступления данных
		if(xQueueReceive(mb_rtu_receive_queue_handle, &hw, portMAX_DELAY) != pdPASS) continue;

		//Проверка Id если Slave
		//
		//Минимальный размер пакета
		//
		//Проверка CRC
		//
		//mb_rtu_send_adu
		//

		//Генерация события о приеме пакета
		//if(hw->ReceiveEvent!=NULL)
		//hw->ReceiveEvent(pdu_buf, len);
		
		//mb_rtu_receive_adu(hw);
		//ulNotifiedValue++;
		osDelay(1);
	}
}

//Прием пакета при помощи DMA
static MB_ErrorRet mb_rtu_receive_adu(MB_RTU_HandleTypeDef *hw)
{
	if (hw == NULL) return MB_ERR_ARG;

	hw->BufLen = 0;
	SET_BIT(hw->uart->Instance->CR2, USART_CR2_RTOEN);
	if (HAL_UART_Receive_DMA(hw->uart, hw->ADU_Buf, MB_HW_ADU_SIZE_MAX) != HAL_OK)
	{
		return MB_ERR_HAL;
	}
	return MB_OK;
}

//Передача пакетов при помощи DMA
static MB_ErrorRet mb_rtu_send_adu(MB_RTU_HandleTypeDef *hw, uint8_t len)
{
	if (hw == NULL) return MB_ERR_ARG;

	hw->BufLen = len;
	if (HAL_UART_Transmit_DMA(hw->uart, hw->ADU_Buf, len) != HAL_OK)
	{
		return MB_ERR_HAL;
	}
	return MB_OK;
}

static MB_RTU_HandleTypeDef * mb_rtu_get_hw_from_uart(UART_HandleTypeDef *huart)
{
	for (int i = 0; i < mb_hw_handles_num; i++)
	{
		if (mb_hw_handles[i]->uart == huart) return mb_hw_handles[i];
	}
	return NULL;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{//Прием завершен - HAL Callback (при нормальной работе эта функция не должна вызываться)
	mb_rtu_receive_adu(mb_rtu_get_hw_from_uart(huart));
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{//Передача завершена - HAL Callback
	mb_rtu_receive_adu(mb_rtu_get_hw_from_uart(huart));	
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

	mb_rtu_receive_adu(mb_rtu_get_hw_from_uart(huart));
}

//Нужно добавить вызов этой функции в stm32f7xx_it.c / USART6_IRQHandler
//Обработчик прерывания от UART (работает вместе с обработчиком HAL), должен
//ловить конец пакета при приеме (по тайм-ауту).
//Эта функция (возможность) в HAL не реализована, но аппаратно в микроконтроллере
//предусмотрена и рассчитана на использование с MODBUS.
void MB_RTU_UART_IRQHandler(UART_HandleTypeDef *huart)
{
	if (__HAL_UART_GET_FLAG(huart, UART_FLAG_RTOF))
	{
		__HAL_UART_CLEAR_IT(huart, UART_CLEAR_RTOF);		
		CLEAR_BIT(huart->Instance->CR2, USART_CR2_RTOEN);
		
		HAL_UART_DMAStop(huart);
		
		MB_RTU_HandleTypeDef *hw = mb_rtu_get_hw_from_uart(huart);
		if (hw == NULL) return;
		
		hw->BufLen = MB_HW_ADU_SIZE_MAX - huart->hdmarx->Instance->NDTR;
		
		if (mb_rtu_receive_task_handle == NULL || mb_rtu_receive_queue_handle == NULL)
		{
			mb_rtu_receive_adu(hw);
			return;
		}

		//Отправка в очередь для дальнейшей обработки
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xQueueSendFromISR(mb_rtu_receive_queue_handle, &hw, &xHigherPriorityTaskWoken);
		//Переключение контекста выполнения (для того чтобы задача обработчик запустилась сразу после прерывания)
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);		
	}
}

