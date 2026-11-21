
#ifndef SWD_TEST_H
#define SWD_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

void SWD_TestReadDPIDCODE(void);
void SWD_TestDPRegisters(void);
void SWD_TestDebugSystemPower(void);

#ifdef __cplusplus
}
#endif

#endif /* SWD_TEST_H */
