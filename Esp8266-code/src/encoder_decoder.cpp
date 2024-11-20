#include "encoder_decoder.h"
#include <stdio.h> // todo: del

uint16_t encode(float temp, int sensor_id) {
	sensor_id = sensor_id > 7 ? 7 : sensor_id;
	sensor_id = sensor_id < 0 ? 0 : sensor_id;

	uint16_t temp_neggative = temp < 0 ? 1 : 0;
	uint16_t temp_enc = temp < 0 ? -temp * 10 : temp * 10;
	temp_enc = temp_enc > 1200 ? 1200 : temp_enc;
	return temp_enc | (temp_neggative << 12) | (sensor_id << 13);
}

float decodeTemp(uint16_t encoded) {
	uint16_t temp_neggative = encoded >> 12 & 1;
	uint16_t temp = encoded & 0b11111111111;
	
	return temp_neggative == 1 ? temp * -0.1 : temp * 0.1;
}

int decodeSensorId(uint16_t encoded) {
	return encoded >> 13;
}