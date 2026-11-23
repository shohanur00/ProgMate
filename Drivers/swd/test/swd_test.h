
#ifndef SWD_TEST_H
#define SWD_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

void SWD_TestReadDPIDCODE(void);
void SWD_TestDPRegisters(void);
void SWD_TestDebugSystemPower(void);
void SWD_TestReadAPIDR(void);
void SWD_TestMemAP(void);
void SWD_TestMemWrite(void);
void SWD_TestFlashUnlock(void);
void SWD_TestMassErase(void);
void SWD_TestFlashProgram(void);

#ifdef __cplusplus
}
#endif

#endif /* SWD_TEST_H */
