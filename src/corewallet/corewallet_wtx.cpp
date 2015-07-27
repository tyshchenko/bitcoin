// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "corewallet/corewallet_wallet.h"
#include "corewallet/corewallet_wtx.h"

#include "consensus/validation.h"
#include "consensus/consensus.h"

#include <algorithm>

namespace CoreWallet {

bool WalletTx::RelayWalletTransaction()
{
    assert(pwallet->GetBroadcastTransactions());
    if (!IsCoinBase())
    {
        if (GetDepthInMainChain() == 0) {
            LogPrintf("Relaying wtx %s\n", GetHash().ToString());
            //RelayTransaction((CTransaction)*this);
            return true;
        }
    }
    return false;
}

int64_t WalletTx::GetTxTime() const
{
    int64_t n = nTimeSmart;
    return n ? n : nTimeReceived;
}

int WalletTx::GetDepthInMainChainInternal() const
{
    if (hashBlock.IsNull() || nIndex == -1)
    {
        if (nHeight == 0)
            return 0;

        return -1; //not in mempool, not in a block
    }

    return pwallet->bestChainTip.nHeight - nHeight + 1;
}

int WalletTx::GetDepthInMainChain() const
{
    return GetDepthInMainChainInternal();
}

int WalletTx::GetBlocksToMaturity() const
{
    if (!IsCoinBase())
        return 0;
    return std::max(0, (COINBASE_MATURITY+1) - GetDepthInMainChain());
}

bool WalletTx::SetMerkleBranch(const CBlock& block)
{
    CBlock blockTmp;

    // Update the tx's hashBlock
    hashBlock = block.GetHash();

    // Locate the transaction
    for (nIndex = 0; nIndex < (int)block.vtx.size(); nIndex++)
        if (block.vtx[nIndex] == *(CTransaction*)this)
            break;
    if (nIndex == (int)block.vtx.size())
    {
        vMerkleBranch.clear();
        nIndex = -1;
        LogPrintf("ERROR: SetMerkleBranch(): couldn't find tx in block\n");
        return false;
    }

    // Fill in merkle branch
    vMerkleBranch = block.GetMerkleBranch(nIndex);
    return true;
}

void WalletTx::SetCache(const enum CREDIT_DEBIT_TYPE &balanceType, const isminefilter& filter, const CAmount &amount) const
{
    std::pair<uint8_t, uint8_t> key(balanceType,filter);
    std::pair<bool, CAmount> value(true, amount);
    cacheMap[key] = value;
}

bool WalletTx::GetCache(const enum CREDIT_DEBIT_TYPE &balanceType, const isminefilter& filter, CAmount &amountOut) const
{
    std::pair<uint8_t, uint8_t> key(balanceType,filter);
    std::pair<bool, CAmount> value = cacheMap[key];
    amountOut = value.second;
    return value.first;
}

bool WalletTx::CheckTransaction(const CTransaction& tx, CValidationState &state)
{
    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        return state.DoS(10, error("CheckTransaction(): vin empty"),
                         REJECT_INVALID, "bad-txns-vin-empty");
    if (tx.vout.empty())
        return state.DoS(10, error("CheckTransaction(): vout empty"),
                         REJECT_INVALID, "bad-txns-vout-empty");
    // Size limits
    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return state.DoS(100, error("CheckTransaction(): size limits failed"),
                         REJECT_INVALID, "bad-txns-oversize");

    // Check for negative or overflow output values
    CAmount nValueOut = 0;
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        if (txout.nValue < 0)
            return state.DoS(100, error("CheckTransaction(): txout.nValue negative"),
                             REJECT_INVALID, "bad-txns-vout-negative");
        if (txout.nValue > MAX_MONEY)
            return state.DoS(100, error("CheckTransaction(): txout.nValue too high"),
                             REJECT_INVALID, "bad-txns-vout-toolarge");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return state.DoS(100, error("CheckTransaction(): txout total out of range"),
                             REJECT_INVALID, "bad-txns-txouttotal-toolarge");
    }

    // Check for duplicate inputs
    std::set<COutPoint> vInOutPoints;
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        if (vInOutPoints.count(txin.prevout))
            return state.DoS(100, error("CheckTransaction(): duplicate inputs"),
                             REJECT_INVALID, "bad-txns-inputs-duplicate");
        vInOutPoints.insert(txin.prevout);
    }

    if (tx.IsCoinBase())
    {
        if (tx.vin[0].scriptSig.size() < 2 || tx.vin[0].scriptSig.size() > 100)
            return state.DoS(100, error("CheckTransaction(): coinbase script size"),
                             REJECT_INVALID, "bad-cb-length");
    }
    else
    {
        BOOST_FOREACH(const CTxIn& txin, tx.vin)
        if (txin.prevout.IsNull())
            return state.DoS(10, error("CheckTransaction(): prevout is null"),
                             REJECT_INVALID, "bad-txns-prevout-null");
    }
    
    return true;
}

}; //end namespace