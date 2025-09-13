#ifndef INC_MATH_H_
#define INC_MATH_H_

#include <stdint.h>

extern float Ro_MQ9;  // global calibration variable

float MQ9_GetRs(uint16_t adc_raw);
float MQ9_GetPPM(float Rs_Ro);


#endif /* INC_MATH_H_ */
