#ifndef SENSOR_H_
#define SENSOR_H_

#include <stdbool.h>

void sensor_init(void);
bool sensor_read(double* temp_c);

#endif /* SENSOR_H_ */
