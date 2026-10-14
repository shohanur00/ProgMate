#ifndef SWD_TEST_H
#define SWD_TEST_H

/*
 * Packet-level test: request words, ACK and parity decoding.
 * No hardware is touched, so it runs without a target connected.
 */
void SWD_TestProtocolPackets(void);

/*
 * Hardware test: SWD_Init -> SWD_Connect -> read DPIDR.
 * Expected on a Cortex-M0+ target: 0x0BC11477.
 */
void SWD_TestReadDPIDCODE(void);

#endif /* SWD_TEST_H */