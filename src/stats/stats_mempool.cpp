// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "stats/stats_mempool.h"

#include "memusage.h"
#include "utiltime.h"

#include "util.h"


const static unsigned int precisionIntervals[] = {
    2, // == every 2 secs == 1800 samples per hour
    60, // == every minute = 1440 samples per day
    1800 // == every half-hour = ~2'160 per Month
};

const static unsigned int MIN_SAMPLES = 10;
const static unsigned int MAX_SAMPLES = 5000;


const static unsigned int fallbackMaxSamplesPerPercision = 1000;

std::atomic<size_t> CStatsMempool::cacheMempoolSize;
std::atomic<size_t> CStatsMempool::cacheMempoolDynamicMemoryUsage;
std::atomic<CAmount> CStatsMempool::cacheMempoolMinRelayFee;

CStatsMempool::CStatsMempool(unsigned int collectIntervalIn) : collectInterval(collectIntervalIn)
{
    startTime = 0;
    intervalCounter = 0;

    // setup the samples per percision vector
    for (unsigned int i = 0; i < sizeof(precisionIntervals) / sizeof(precisionIntervals[0]); i++) {
        vSamplesPerPrecision.push_back(std::make_shared<MempoolSamplesVector>());

        // use the fallback max in case max memory will not be set
        vMaxSamplesPerPrecision.push_back(fallbackMaxSamplesPerPercision);

        // add starttime 0 to each level
        vTimeLastSample.push_back(0);
    }
}

std::vector<unsigned int> CStatsMempool::getPercisionGroupsAndIntervals() {
    std::vector<unsigned int> grps;
    for (unsigned int i = 0; i < sizeof(precisionIntervals) / sizeof(precisionIntervals[0]); i++) {
        grps.push_back(precisionIntervals[i]);
    }
    return grps;
}

bool CStatsMempool::addMempoolSamples(const size_t maxStatsMemory)
{
    bool statsChanged = false;
    uint64_t now = GetTime();
    {
        LOCK(cs_mempool_stats);

        // set the mempool stats start time if this is the first sample
        if (startTime == 0)
            startTime = now;

        unsigned int biggestInterval = 0;
        for (unsigned int i = 0; i < sizeof(precisionIntervals) / sizeof(precisionIntervals[0]); i++) {
            // check if it's time to collect a samples for the given percision level
            uint16_t timeDelta = 0;
            if (intervalCounter % (precisionIntervals[i] / (collectInterval / 1000)) == 0) {
                if (vTimeLastSample[i] == 0) {
                    // first sample, calc delta to starttime
                    timeDelta = now - startTime;
                } else {
                    timeDelta = now - vTimeLastSample[i];
                }
                printf("Stats %lld %d\n", now, timeDelta);
                vSamplesPerPrecision[i]->push_back({timeDelta, CStatsMempool::cacheMempoolSize, CStatsMempool::cacheMempoolDynamicMemoryUsage, CStatsMempool::cacheMempoolMinRelayFee});
                statsChanged = true;

                // check if we need to remove items at the beginning
                if (vSamplesPerPrecision[i]->size() > vMaxSamplesPerPrecision[i]) {
                    // increase starttime by the removed deltas
                    for (unsigned int j = (vSamplesPerPrecision[i]->size() - vMaxSamplesPerPrecision[i]); j > 0; j--) {
                        startTime += (*vSamplesPerPrecision[i])[j].timeDelta;
                    }
                    // remove element(s) at vector front
                    vSamplesPerPrecision[i]->erase(vSamplesPerPrecision[i]->begin(), vSamplesPerPrecision[i]->begin() + (vSamplesPerPrecision[i]->size() - vMaxSamplesPerPrecision[i]));

                    // release memory
                    vSamplesPerPrecision[i]->shrink_to_fit();
                }

                vTimeLastSample[i] = now;
            }
            biggestInterval = precisionIntervals[i];
        }

        intervalCounter++;

        if (intervalCounter == biggestInterval) {
            intervalCounter = 0;
        }
    }
    return statsChanged;
}

void CStatsMempool::setMaxMemoryUsageTarget(size_t maxMem)
{
    // calculate the memory requirement of a single sample
    size_t sampleSize = memusage::MallocUsage(sizeof(CStatsMempoolSample));

    // calculate how many samples would fit in the target
    size_t maxAmountOfSamples = maxMem / sampleSize;

    // distribute the max samples equal between percision levels
    unsigned int samplesPerPercision = maxAmountOfSamples / sizeof(precisionIntervals) / sizeof(precisionIntervals[0]);
    samplesPerPercision = std::max(MIN_SAMPLES, samplesPerPercision);
    samplesPerPercision = std::min(MAX_SAMPLES, samplesPerPercision);
    for (unsigned int i = 0; i < sizeof(precisionIntervals) / sizeof(precisionIntervals[0]); i++) {
        vMaxSamplesPerPrecision[i] = samplesPerPercision;
    }
}

MempoolSamplesVectorRef CStatsMempool::getSamplesForPercision(unsigned int percision, uint64_t& fromTime)
{
    LOCK(cs_mempool_stats);

    if (percision >= vSamplesPerPrecision.size()) {
        return nullptr;
    }

    fromTime = startTime;
    return vSamplesPerPrecision[percision];
}
