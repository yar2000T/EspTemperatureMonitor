#include <measurement_store_test.hpp>
#include <encoding_decoding_test.hpp>
#include <unity.h>
#include <stdio.h>

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(encode_test);
  RUN_TEST(decodeSensorId_test);
  RUN_TEST(decodeTemperature_test);
  RUN_TEST(test_saveTemperature);
  RUN_TEST(test_initFunct);
  RUN_TEST(test_readTemperature);
  UNITY_END();

}
