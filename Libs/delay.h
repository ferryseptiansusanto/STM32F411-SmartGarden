#ifndef DELAY_H_
#define DELAY_H_

#include "main.h"
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

// Inisialisasi Hardware Cycle Counter (Panggil sekali di main.c sebelum while(1))
void DWT_Delay_Init(void);

uint32_t GetTick(void);
void DelayMs(uint32_t ms);
void DelayUs(uint32_t us);


#endif /* DELAY_H_ */
