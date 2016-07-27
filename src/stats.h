// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_STATS_H
#define BITCOIN_STATS_H

#include <sync.h>

#include <stdlib.h>
#include <vector>

#include <boost/signals2/signal.hpp>

struct CStatsMempoolSample
{
    uint32_t timeDelta; //use 32bit time delta to save memmory
    int64_t txCount; //transaction count
    int64_t dynMemUsage; //dynamic mempool usage
    int64_t minFeePerK; //min fee per K
};

typedef std::vector<struct CStatsMempoolSample> mempoolSamples_t;

// simple mempool stats container
class CStatsMempool
{
public:
    uint64_t startTime; //start time
    mempoolSamples_t vSamples;
    uint64_t cleanupCounter; //internal counter for cleanups

    CStatsMempool() {
        startTime = 0;
        cleanupCounter = 0;
    }
};

class CStats
{
private:
    static const uint32_t SAMPLE_MIN_DELTA_IN_SEC; //minimum delta in seconds between samples
    static const int CLEANUP_SAMPLES_THRESHOLD; //amount of samples until we perform a cleanup (remove old samples)
    static size_t MAX_STATS_MEM; //maximum amount of memory to use for the stats

    static CStats *sharedInstance;
    mutable CCriticalSection cs_stats;

    CStatsMempool mempoolStats; //mempool stats container

public:
    static CStats* DefaultStats(); //shared instance
    void addMempoolSample(int64_t txcount, int64_t dynUsage, int64_t currentMinRelayFee);
    mempoolSamples_t mempoolGetValuesInRange(uint64_t &fromTime, uint64_t &toTime, int intervalSec);

    /* set the target for the maximum memory consuption (in bytes) */
    void setMaxMemoryUsageTarget(size_t maxMem);
    /* signals */
    boost::signals2::signal<void (void)> MempoolStatsDidChange;
};

#endif // BITCOIN_STATS_H
