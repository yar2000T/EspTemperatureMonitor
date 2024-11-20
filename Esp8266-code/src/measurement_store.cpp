#include "encoder_decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

int16_t *buffTempsAndSensors;
unsigned long *buffTimes;
int buffSize;
int currentIndex = 0;

struct SensTempTime {
    float temp;
    unsigned long time;
    int sensor_id;
};

void setBuff(int16_t *buffTempsAndSensors2, unsigned long *buffTimes2, int buffsize2, int index) {
    currentIndex = index;
    buffTempsAndSensors = buffTempsAndSensors2;
    buffTimes = buffTimes2;
    buffSize = buffsize2;
}

void initBuff(unsigned int buffsize2) {
    buffTempsAndSensors = (int16_t*)malloc(buffsize2 * sizeof(int16_t));
    buffTimes = (unsigned long*)malloc(buffsize2 * sizeof(unsigned long));
    
    if (buffTempsAndSensors == NULL || buffTimes == NULL) {
        return;
    }

    for (int i = 0; i < buffsize2; i++) {
        buffTempsAndSensors[i] = 0;
        buffTimes[i] = 0;
    }
    
    setBuff(buffTempsAndSensors, buffTimes, buffsize2, 0);
}

void saveTemperature(float temp, unsigned long time, unsigned int sensorId, float maxTempDiff) {
    int16_t encoded = encode(temp, sensorId);
    int lastIndex = -1;
    int secondLastIndex = -1;
    int index = (currentIndex - 1 + buffSize) % buffSize;

    for (int counter = 0; counter < buffSize; counter++) {
        if (decodeSensorId(buffTempsAndSensors[index]) == sensorId) {
            secondLastIndex = lastIndex;
            lastIndex = index;
        }
        if (secondLastIndex != -1 && lastIndex != -1) break;
        index = (index - 1 + buffSize) % buffSize;
    }

    float diffLast = (lastIndex != -1) ? fabs(temp - decodeTemp(buffTempsAndSensors[lastIndex])) : FLT_MAX;
    float diffSecondLast = (secondLastIndex != -1) ? fabs(temp - decodeTemp(buffTempsAndSensors[secondLastIndex])) : FLT_MAX;

    if (secondLastIndex == -1 || lastIndex == -1 || 
        (time - buffTimes[secondLastIndex] >= 300000) || 
        (diffLast > maxTempDiff || diffSecondLast > maxTempDiff)) {
        
        buffTempsAndSensors[currentIndex] = encoded;
        buffTimes[currentIndex] = time;
        currentIndex = (currentIndex + 1) % buffSize;
    } else {
        int updateIndex = (currentIndex - 1 + buffSize) % buffSize;
        if (decodeSensorId(buffTempsAndSensors[updateIndex]) == sensorId) {
            buffTimes[updateIndex] = time;
        } else if (secondLastIndex != -1) {
            buffTimes[secondLastIndex] = time;
        }
    }
}

SensTempTime getTemp(int index) {
    SensTempTime result;
    result.temp = 0.0f;
    result.time = 0;
    result.sensor_id = -1;
    int actualIndex = (currentIndex - index + buffSize) % buffSize;

    if (actualIndex >= 0) {
        result.temp = decodeTemp(buffTempsAndSensors[actualIndex]);
        result.time = buffTimes[actualIndex];
        result.sensor_id = decodeSensorId(buffTempsAndSensors[actualIndex]);
    }

    return result;
}
