#ifndef INC_ADC_H_
#define INC_ADC_H_

void ADC_Init();
uint16_t ADC_ReadChannel(uint8_t channel);
float ADC_ReadTempSensor(void);
float ADC_ReadVref(void);

#endif
