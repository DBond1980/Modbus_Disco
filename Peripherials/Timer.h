#ifndef __TIMER_H__
#define __TIMER_H__

#define TIMER_MEASINTERVALS_ACTIVE  1

void Timer_Init(void);
uint32_t Timer_GetCnt_us(void);
uint32_t Timer_GetCnt_ms(void);
void Timer_Delay_us(uint32_t delay);

#if TIMER_MEASINTERVALS_ACTIVE == 1
void Timer_MeasInterval_Begin(uint8_t ch);
void Timer_MeasInterval_End(uint8_t ch);

#define TIMER_MEAS_BEGIN(ch)	Timer_MeasInterval_Begin(ch)
#define TIMER_MEAS_END(ch)		Timer_MeasInterval_End(ch)

#else

#define TIMER_MEAS_BEGIN(ch)		
#define TIMER_MEAS_END(ch)		

#endif


#endif  /* __TIMER_H__ */


