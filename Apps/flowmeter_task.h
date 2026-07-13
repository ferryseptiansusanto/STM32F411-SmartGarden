/*
 * flowmeter_task.h
 *
 *  Created on: 11 Jun 2026
 *      Author: ferry
 */

#ifndef INC_TASKS_FLOWMETER_TASK_H_
#define INC_TASKS_FLOWMETER_TASK_H_

void Flowmeter_TaskCreate(SPI_Context *Storage_Ctx, I2C_RTCDevice *Rtc_Ctx, UBaseType_t priority);

#endif /* INC_TASKS_FLOWMETER_TASK_H_ */
