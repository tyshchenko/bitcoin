
#include "poolman.h"
#include "main.h"
#include "init.h"
#include "util.h"
#include "utiltime.h"
#include "validationinterface.h"

using namespace std;

int64_t janitorExpire; // global; expire TXs n seconds older than this
int64_t janitorInterval;

void TxMempoolJanitor()
{
    int64_t nStart = GetTimeMillis();
    int64_t expireTime = GetTime() - janitorExpire;

    // pass 1: get matching transactions
    vector<CTransaction> vtx;
    mempool.queryOld(vtx, expireTime);
    unsigned int nOld = vtx.size();

    // pass 2: allow listening validation interfaces to remove transations from the vector
    unsigned int nMine = 0;
    GetMainSignals().PrepareMempoolCleanup(vtx, nMine);

    // pass 3: remove old transactions from mempool
    bool fRecursive = false;
    std::list<CTransaction> removed;
    mempool.removeBatch(vtx, removed, fRecursive);

    LogPrint("mempool", "mempool janitor run complete,  %dms\n",
             GetTimeMillis() - nStart);
    LogPrint("mempool", "    %u old, %u removed\n",
             nOld, nMine, removed.size());
}

