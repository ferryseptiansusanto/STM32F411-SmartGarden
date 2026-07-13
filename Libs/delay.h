#ifndef DELAY_H_
#define DELAY_H_


#include "main.h"
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"


uint32_t GetTick(void);
// Delay dalam milidetik (non-blocking, multitasking friendly)
void DelayMs(uint32_t ms);

// Delay mikrodetik (blocking, hanya untuk kebutuhan singkat)
void DelayUs(uint32_t us);

#endif /* DELAY_H_ */
