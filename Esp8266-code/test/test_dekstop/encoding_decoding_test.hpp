#include <unity.h>
#include "encoder_decoder.h"
#include <stdio.h>

void zero_test(void) {
    uint16_t temp = 0.00;
    uint16_t id = 1;

    uint16_t result = encode(temp,id);
    TEST_ASSERT_EQUAL(id, decodeSensorId(result));

    TEST_ASSERT_EQUAL(temp, decodeTemp(result));
}

void positive_value_test(void) {
    uint16_t temp = 17.50;
    uint16_t id = 1;

    uint16_t result = encode(temp,id);
    TEST_ASSERT_EQUAL(id, decodeSensorId(result));

    TEST_ASSERT_EQUAL(temp, decodeTemp(result));
}

void encode_test(void) {
    TEST_ASSERT_EQUAL((uint16_t)0b0011000010101111, encode(-17.5, 1));
    TEST_ASSERT_EQUAL((uint16_t)0b1010000000000000, encode(0, 5));
    TEST_ASSERT_EQUAL((uint16_t)0b0100001101000101, encode(83.76, 2));
    TEST_ASSERT_EQUAL((uint16_t)0b0110010010100110, encode(119, 3));
    TEST_ASSERT_EQUAL((uint16_t)0b1000010010110000, encode(120, 4));
    TEST_ASSERT_EQUAL((uint16_t)0b1100010010110000, encode(121, 6)); // temperature cannot be greater than 120
    TEST_ASSERT_EQUAL((uint16_t)0b1111010010100110, encode(-119, 7));
    TEST_ASSERT_EQUAL((uint16_t)0b1111010010110000, encode(-120, 8)); // sensor_id cannot be greater than 7 
    TEST_ASSERT_EQUAL((uint16_t)0b1111010010110000, encode(-121, 100)); // temperature cannot be less than -120
    TEST_ASSERT_EQUAL((uint16_t)0b0000000000000000, encode(0, -5));
}

void decodeSensorId_test() {
    TEST_ASSERT_EQUAL(3, decodeSensorId((uint16_t)0b0111010010110000));
    TEST_ASSERT_EQUAL(7, decodeSensorId((uint16_t)0b1111010010110000));
    TEST_ASSERT_EQUAL(1, decodeSensorId((uint16_t)0b0011010010110000));
    TEST_ASSERT_EQUAL(0, decodeSensorId((uint16_t)0b0001010010110000));
}

void decodeTemperature_test(void) {
    TEST_ASSERT_EQUAL(-17.5, decodeTemp((uint16_t)0b0001000010101111));
    TEST_ASSERT_EQUAL(120, decodeTemp((uint16_t)0b0000010010110000));
    TEST_ASSERT_EQUAL(-119, decodeTemp((uint16_t)0b1111010010100110));
    TEST_ASSERT_EQUAL(-120, decodeTemp((uint16_t)0b1111010010110000));
    TEST_ASSERT_EQUAL(0, decodeTemp((uint16_t)0b0000000000000000));
}
