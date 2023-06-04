#ifndef TEST_PROBE_H
#define TEST_PROBE_H
//#define TEST_MODE

#define TEST_PROBE_1 18
#define TEST_PROBE_2 5
#define TEST_PROBE_3 4

#ifdef TEST_PROBE_1
#define PROBE_1(x) digitalWrite(TEST_PROBE_1, x)
#define PROBE_1_TOGGLE           \
  pinMode(TEST_PROBE_1, OUTPUT); \
  digitalWrite(TEST_PROBE_1, digitalRead(TEST_PROBE_1) == HIGH ? LOW : HIGH)
#else
#define PROBE_1(x)
#define PROBE_1_TOGGLE
#endif
#ifdef TEST_PROBE_2
#define PROBE_2(x) digitalWrite(TEST_PROBE_2, x)
#define PROBE_2_TOGGLE           \
  pinMode(TEST_PROBE_2, OUTPUT); \
  digitalWrite(TEST_PROBE_2, digitalRead(TEST_PROBE_2) == HIGH ? LOW : HIGH)
#else
#define PROBE_2(x)
#define PROBE_2_TOGGLE
#endif
#ifdef TEST_PROBE_3
#define PROBE_3(x) digitalWrite(TEST_PROBE_3, x)
#define PROBE_3_TOGGLE           \
  pinMode(TEST_PROBE_3, OUTPUT); \
  digitalWrite(TEST_PROBE_3, digitalRead(TEST_PROBE_3) == HIGH ? LOW : HIGH)
#else
#define PROBE_3(x)
#define PROBE_3_TOGGLE
#endif

#endif