#ifndef SWD_TEST_H
#define SWD_TEST_H

/*
 * Packet-level test: request words, ACK and parity decoding.
 * No hardware is touched, so it runs without a target connected.
 */
void SWD_TestProtocolPackets(void);



#endif /* SWD_TEST_H */