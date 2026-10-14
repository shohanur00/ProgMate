#ifndef SWD_TYPES_H
#define SWD_TYPES_H

/* Status code shared by every SWD layer. */
typedef enum
{
    SWD_STATUS_OK = 0,
    SWD_STATUS_WAIT,        /* Target answered WAIT, retries exhausted      */
    SWD_STATUS_FAULT,       /* Target answered FAULT                        */
    SWD_STATUS_NO_ACK,      /* ACK was 0x7, 0x0 or otherwise invalid        */
    SWD_STATUS_PARITY,      /* Read data parity mismatch                    */
    SWD_STATUS_TIMEOUT,     /* Polled condition did not occur in time       */
    SWD_STATUS_PARAM,       /* Invalid argument                             */
    SWD_STATUS_HW_ERROR     /* Hardware / PHY initialization failed         */
} SWD_Status_t;

#endif /* SWD_TYPES_H */