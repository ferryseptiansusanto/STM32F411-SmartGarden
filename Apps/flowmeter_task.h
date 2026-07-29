/*
 * flowmeter_task.h
 *
 *  Created on: 11 Jun 2026
 *      Author: ferry
 */

#ifndef INC_TASKS_FLOWMETER_TASK_H_
#define INC_TASKS_FLOWMETER_TASK_H_

#include "flowmeter/flowmeter_driver.h"

extern FlowSensor_t fm_inlet;
extern FlowSensor_t fm_outlet;
extern FlowSensor_t fm_fert;

void Flowmeter_TaskCreate(UBaseType_t priority);

#endif /* INC_TASKS_FLOWMETER_TASK_H_ */
