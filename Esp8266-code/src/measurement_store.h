#ifndef MEASUREMENT_STORE_H
#define MEASUREMENT_STORE_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include "encoder_decoder.h"

struct SensTempTime {
    float temp;
    unsigned long time;
    int sensor_id;
};

void setBuff(int16_t *buffTempsAndSensors2, unsigned long *buffTimes2, int buffsize2, int index);
void initBuff(unsigned int buffsize2);
void saveTemperature(float temp, unsigned long time, unsigned int sensorId, float maxTempDiff);
struct SensTempTime getTemp(int index);

#endif
