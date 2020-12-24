#include "stm32f7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include "Timer.h"

extern TIM_HandleTypeDef htim5;

extern __IO uint32_t uwTick;

void Timer_Init(void)
{
	HAL_TIM_Base_Start(&htim5);
}

uint32_t Timer_GetCnt_us(void)
{
	return __HAL_TIM_GET_COUNTER(&htim5);
}

uint32_t Timer_GetCnt_ms(void)
{
	return uwTick;
}

void Timer_Delay_us(uint32_t delay)
{
	uint32_t cnt = __HAL_TIM_GET_COUNTER(&htim5);

	while (__HAL_TIM_GET_COUNTER(&htim5) - cnt < delay) ;
}

#if TIMER_MEASINTERVALS_ACTIVE == 1

#define TIMER_MI_CH_NUM		5
#define TIMER_MI_SIZE		10
uint32_t Timer_MI_Data[TIMER_MI_CH_NUM][TIMER_MI_SIZE];
uint32_t Timer_MI_Cnt[TIMER_MI_CH_NUM];
uint16_t Timer_MI_Index[TIMER_MI_CH_NUM];
void Timer_MeasInterval_Begin(uint8_t ch)
{
	Timer_MI_Cnt[ch] = __HAL_TIM_GET_COUNTER(&htim5);
}
void Timer_MeasInterval_End(uint8_t ch)
{
	Timer_MI_Data[ch][Timer_MI_Index[ch]] = __HAL_TIM_GET_COUNTER(&htim5) - Timer_MI_Cnt[ch];
	Timer_MI_Index[ch]++;
	if (Timer_MI_Index[ch] >= TIMER_MI_SIZE) Timer_MI_Index[ch] = 0;
}

#endif

