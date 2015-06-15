#ifndef __BITCOIN_POOLMAN_H__
#define __BITCOIN_POOLMAN_H__

#include <stdint.h>

extern void TxMempoolJanitor();
extern int64_t janitorExpire;
extern int64_t janitorInterval;


#endif // __BITCOIN_POOLMAN_H__
