/*
 * utils.h
 *
 *  Created on: 29 Apr 2026
 *      Author: ferry
 */

#ifndef UTILS_H_
#define UTILS_H_
#include "main.h"

uint8_t bcd2dec(uint8_t val);
uint8_t dec2bcd(uint8_t val);
void float_to_string_dynamic(float value, char *buffer, int decimal);
void vAssertCalled(const char *file, int line) ;


#endif /* UTILS_H_ */
