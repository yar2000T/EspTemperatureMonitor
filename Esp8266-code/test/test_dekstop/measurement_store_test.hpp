#include <unity.h>
#include <measurement_store.h>
// #include "encoder_decoder.h"
#include <stdio.h>
#include <array>

const int maxRecords = 4;

typedef struct {
    float initTemps[maxRecords];
    unsigned long initTimes[maxRecords];
    int initSensorIds[maxRecords];
    float newTemp;
    unsigned long newTime;
    int sensorId;
    float expectedTemps[maxRecords];
    unsigned long expectedTimes[maxRecords];
    int expectedSensorIds[maxRecords];
} SaveTestCases;

typedef struct {
    float initTemps[maxRecords];
    unsigned long initTimes[maxRecords];
    int initSensorIds[maxRecords];
    float index;
    float expectedTemp;
    unsigned long expectedTime;
    int expectedSensorId;
} ReadTestCases;

void validateBuff(float initTemps[], unsigned long initTimes[], int initSensorIds[], 
                  float newTemp, unsigned long newTime, int sensorId, 
                  float expectedTemps[], unsigned long expectedTimes[], 
                  int expectedSensorIds[]) {
    int16_t buff[maxRecords];
    unsigned long timestamps[maxRecords];

    for (int i = 0; i < maxRecords; i++) {
        buff[i] = encode(initTemps[i], initSensorIds[i]);
        timestamps[i] = initTimes[i];
    }

    unsigned int currentIndex = 0;
    unsigned long maxTime = 0;

    for (int i = 0; i < maxRecords; i++) {
        if (timestamps[i] >= maxTime) {
            currentIndex = (currentIndex + 1) == maxRecords ? 0 : i + 1;     
            maxTime = timestamps[i];
        }
    }

    setBuff(buff, timestamps, maxRecords, currentIndex);
    saveTemperature(newTemp, newTime, sensorId, 0.2);

    for (int i = 0; i < maxRecords; i++) {
        float actualTemp = decodeTemp(buff[i]);
        unsigned long actualTime = timestamps[i];
        unsigned int actualSensorId = decodeSensorId(buff[i]);

        TEST_ASSERT_EQUAL(expectedTemps[i], actualTemp);
        TEST_ASSERT_EQUAL(expectedTimes[i], actualTime);
        TEST_ASSERT_EQUAL(expectedSensorIds[i], actualSensorId);
    }
}


void validateReadBuff(float initTemps[], unsigned long initTimes[], int initSensorIds[],
                      int index, float expectedTemp, unsigned long expectedTime, 
                      int expectedSensorId) {
    int16_t buff[maxRecords];
    unsigned long timestamps[maxRecords];

    for (int i = 0; i < maxRecords; i++) {
        buff[i] = encode(initTemps[i], initSensorIds[i]);
        timestamps[i] = initTimes[i];
    }

    unsigned int currentIndex = 0;
    unsigned long maxTime = 0;

    for (int i = 0; i < maxRecords; i++) {
        if (timestamps[i] >= maxTime) {
            currentIndex = (currentIndex + 1) == maxRecords ? 0 : i + 1;     
            maxTime = timestamps[i];
        }
    }

    setBuff(buff, timestamps, maxRecords, currentIndex);
    SensTempTime temperatureData = getTemp(index);

    TEST_ASSERT_EQUAL(expectedTemp, temperatureData.temp);
    TEST_ASSERT_EQUAL(expectedTime, temperatureData.time);
    TEST_ASSERT_EQUAL(expectedSensorId, temperatureData.sensor_id);
}

void runSaveTest(SaveTestCases scenario) {
    validateBuff(scenario.initTemps, scenario.initTimes, scenario.initSensorIds,
                    scenario.newTemp, scenario.newTime, scenario.sensorId,
                    scenario.expectedTemps, scenario.expectedTimes, scenario.expectedSensorIds);
}

void runReadTest(ReadTestCases scenario) {
    validateReadBuff(scenario.initTemps, scenario.initTimes, scenario.initSensorIds,
                    scenario.index,
                    scenario.expectedTemp, scenario.expectedTime, scenario.expectedSensorId);
}

void test_saveTemperature(void) { 
    // Empty list

    runSaveTest(
        {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        25.300000000, 1000, 1,
        {25.300000000, 0, 0, 0}, {1000, 0, 0, 0}, {1, 0, 0, 0}}
    );

    runSaveTest(
        {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
        25.3, 1000, 1,
        {25.3, 0, 0, 0}, {1000, 0, 0, 0}, {1, 0, 0, 0}}
    );

    // Single sensor_id test
    runSaveTest(
        {{25.2, 25.2, 25.2, 0}, {10, 50, 100, 0}, {1, 1, 1, 0}, 
         25.3, 150, 1, 
         {25.2, 25.2, 25.2, 0}, {10, 50, 150, 0}, {1, 1, 1, 0}}
    );

    // 2 sensor_id test
    runSaveTest(
        {{26.2, 26.1, 26.2, 0}, {20, 60, 200, 0}, {2, 1, 2, 0}, 
        26.3, 250, 2, 
        {26.2, 26.1, 26.2, 0}, {20, 60, 250, 0}, {2, 1, 2, 0}}
    );

    // Single sensor_id, new record with limit of time (300000 millis.) test
    runSaveTest(
        {{26.1, 26.1, 26.2, 0}, {20, 60, 200, 0}, {2, 1, 2, 0}, 
        26.3, 300200, 2, 
        {26.1, 26.1, 26.2, 26.3}, {20, 60, 200, 300200}, {2, 1, 2, 2}}
    );

    // Full list but have a new temperature(use previous record)
    runSaveTest(
        {{26.0, 26.1, 26.2, 26.2}, {20, 60, 100, 200}, {2, 2, 2, 2}, 
        26.3, 300, 2, 
        {26.0, 26.1, 26.2, 26.3}, {20, 60, 100, 300}, {2, 2, 2, 2}}
    );

    // Full list but have a new temperature(insert new record)
    runSaveTest(
        {{26.0, 26.1, 26.2, 26.2}, {20, 60, 200, 510}, {2, 2, 2, 2}, 
        26.3, 610, 1, 
        {26.3, 26.1, 26.2, 26.3}, {610, 60, 200, 510}, {1, 2, 2, 2}}
    );


    //Multiple sensor IDs, edge case with identical timestamps
    runSaveTest(
        {{24.3, 24.1, 24.3, 24.3}, {10, 10, 20, 20}, {1, 2, 1, 2}, 
         24.4, 30, 1,
         {24.3, 24.1, 24.3, 24.4}, {10, 10, 30, 20}, {1, 2, 1, 2}}
    );
}

void test_readTemperature(void) {
    runReadTest({{}, {}, {},
        0,
        0.0, 0, 0});


    runReadTest({{17.5, 24.4, 18.1, 24.5}, {10, 10, 20 ,20}, {1, 2, 1, 2},
                1,
                24.5, 20, 2});
    
    runReadTest({{17.5, 24.4, 18.1, 24.5}, {10, 10, 20 ,20}, {1, 2, 1, 2},
                2,
                18.1, 20, 1});
    
    runReadTest({{17.5, 24.4, 18.1, 24.5}, {10, 10, 20 ,20}, {1, 2, 1, 2},
                3,
                24.4, 10, 2});
}

void test_initFunct(void) {
    initBuff(20);

    saveTemperature(24.3, 10, 1, 0.2);
    saveTemperature(27.1, 10, 2, 0.2);

    for (int i = 1; i < 20; i++) {
        SensTempTime data = getTemp(i);

        if (i != 1) {
            TEST_ASSERT_EQUAL(0, data.temp);
            TEST_ASSERT_EQUAL(0, data.time);
            TEST_ASSERT_EQUAL(0, data.sensor_id);
        }else{
            TEST_ASSERT_EQUAL(24.3, data.temp);
            TEST_ASSERT_EQUAL(10, data.time);
            TEST_ASSERT_EQUAL(1, data.sensor_id);
        }
    }
}