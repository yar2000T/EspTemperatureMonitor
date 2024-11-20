#ifndef ENCODER_DECODER
#define ENCODER_DECODER

#include <stdint.h>

uint16_t encode(float temp, int sensor_id);

float decodeTemp(uint16_t encoded);

int decodeSensorId(uint16_t encoded);

#endif