// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "corewallet/corewallet.h"
#include "corewallet/corewallet_db.h"
#include "corewallet/corewallet_wallet.h"
#include "main.h"
#include "rpcserver.h"
#include "script/script.h"
#include "ui_interface.h"
#include "util.h"
#include "validationinterface.h"

#include <string>

#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>

namespace CoreWallet {

const static std::string DEFAULT_WALLETS_METADATA_FILE = "multiwallet.dat";
static Manager *managerSharedInstance;

//implemented in corewallet_rpc.cpp
extern void ExecuteRPC(const std::string& strMethod, const UniValue& params, UniValue& result, bool& accept);


bool CheckFilenameString(const std::string& str)
{
    static std::string safeChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890._-");
    std::string strResult;
    for (std::string::size_type i = 0; i < str.size(); i++)
    {
        if (safeChars.find(str[i]) == std::string::npos)
            return false;
    }
    return true;
}
    
void AppendHelpMessageString(std::string& strUsage, bool debugHelp)
{
    if (debugHelp)
        return;
    
    strUsage += HelpMessageGroup(_("CoreWallet options:"));
    strUsage += HelpMessageOpt("-disablecorewallet", _("Do not load the wallet and disable wallet RPC calls"));
}

Manager::Manager()
{

}

bool Manager::LoadWallets(std::string& warningString, std::string& errorString)
{
    int64_t nStart = GetTimeMillis();
    ReadWalletLists();

    bool allWalletsLoaded = true;
    std::pair<std::string, WalletModel> walletAndMetadata;
    std::vector<CBlockLocator> walletsBestBlocks;

    LOCK2(cs_main, cs_mapWallets);
    BOOST_FOREACH(walletAndMetadata, mapWallets)
    if (!mapWallets[walletAndMetadata.first].pWallet) {
        Wallet *newWallet = new Wallet(walletAndMetadata.first);
        if (!newWallet->LoadWallet(warningString, errorString)) {
            allWalletsLoaded = false;
        }
        else
        {
            walletsBestBlocks.push_back(newWallet->lastKnowBestBlock);
        }
        mapWallets[walletAndMetadata.first].pWallet = newWallet;
    }

    CBlockIndex *pindexRescan = chainActive.Tip();
    if (GetBoolArg("-rescan", false)) {
        pindexRescan = chainActive.Genesis();
    }
    else
    {
        // search the blockindex with lowest height including a check
        // if blocklocator is in chainActive
        pindexRescan = chainActive.Genesis();
        CBlockIndex *pTempBlockIndex;
        BOOST_FOREACH(CBlockLocator locator, walletsBestBlocks)
        {
            if (locator.IsNull())
                break;

            pTempBlockIndex = FindForkInGlobalIndex(chainActive, locator);
            if (pTempBlockIndex->nHeight < pindexRescan->nHeight && pTempBlockIndex != chainActive.Genesis())
                pindexRescan = pTempBlockIndex;
        }
    }
    if (chainActive.Tip() && chainActive.Tip() != pindexRescan)
    {
        //We can't rescan beyond non-pruned blocks, stop and throw an error
        //this might happen if a user uses a old wallet within a pruned node
        // or if he ran -disablewallet for a longer time, then decided to re-enable
        if (fPruneMode)
        {
            CBlockIndex *block = chainActive.Tip();
            while (block && block->pprev && (block->pprev->nStatus & BLOCK_HAVE_DATA) && block->pprev->nTx > 0 && pindexRescan != block)
                block = block->pprev;

            if (pindexRescan != block)
            {
                errorString += _("Prune: last wallet synchronisation goes beyond pruned data. You need to -reindex (download the whole blockchain again in case of pruned node)") + "\n";
                return true;
            }
        }

        uiInterface.InitMessage(_("Rescanning..."));
        LogPrintf("Rescanning last %i blocks (from block %i)...\n", chainActive.Height() - pindexRescan->nHeight, pindexRescan->nHeight);
        nStart = GetTimeMillis();
        this->RequestWalletRescan(pindexRescan, true);
        LogPrintf(" rescan      %15dms\n", GetTimeMillis() - nStart);

        BOOST_FOREACH(walletAndMetadata, mapWallets)
        {
            Wallet *pWallet = mapWallets[walletAndMetadata.first].pWallet;
            if (pWallet) {
                pWallet->SetBestChain(chainActive.GetLocator());
            }
        }
    }

    return allWalletsLoaded;
}

void Manager::ReadWalletLists()
{
    CAutoFile multiwalletFile(fopen((GetDataDir() / DEFAULT_WALLETS_METADATA_FILE).string().c_str(), "rb"), SER_DISK, CLIENT_VERSION);
    if (!multiwalletFile.IsNull())
    {
        try {
            LOCK2(cs_main, cs_mapWallets);
            multiwalletFile >> mapWallets;
        } catch (const std::exception&) {
            LogPrintf("CoreWallet: could not read multiwallet metadata file (non-fatal)");
        }
    }
}

void Manager::WriteWalletList()
{
    CAutoFile multiwalletFile(fopen((GetDataDir() / DEFAULT_WALLETS_METADATA_FILE).string().c_str(), "wb"), SER_DISK, CLIENT_VERSION);
    if (!multiwalletFile.IsNull())
    {
        LOCK2(cs_main, cs_mapWallets);
        multiwalletFile << mapWallets;
    }
}

void LoadAsModule(std::string& warningString, std::string& errorString, bool& stopInit)
{
    if (!GetManager()->LoadWallets(warningString, errorString)) {
        stopInit = true;
    }
}

Wallet* Manager::AddNewWallet(const std::string& walletID)
{
    Wallet *newWallet = NULL;
    LOCK2(cs_main, cs_mapWallets);
    {
        if (mapWallets.find(walletID) != mapWallets.end())
            throw std::runtime_error(_("walletid already exists"));
        
        if (!CheckFilenameString(walletID))
            throw std::runtime_error(_("wallet ids can only contain A-Za-z0-9._- chars"));
        
        newWallet = new Wallet(walletID);
        std::string strError,strWarning;
        newWallet->LoadWallet(strError, strWarning);
        mapWallets[walletID] = WalletModel(walletID, newWallet);
    }

    WriteWalletList();
    return newWallet;
}

Wallet* Manager::GetWalletWithID(const std::string& walletIDIn)
{
    std::string walletID = walletIDIn;

    LOCK2(cs_main, cs_mapWallets);
    {
        if (walletID == "" && mapWallets.size() == 1)
            walletID = mapWallets.begin()->first;

        if (mapWallets.find(walletID) != mapWallets.end())
        {
            if (!mapWallets[walletID].pWallet) //is it closed?
                mapWallets[walletID].pWallet = new Wallet(walletID);

            return mapWallets[walletID].pWallet;
        }
    }
    
    return NULL;
}

std::vector<std::string> Manager::GetWalletIDs()
{
    std::vector<std::string> vIDs;
    std::pair<std::string, WalletModel> walletAndMetadata;

    LOCK2(cs_main, cs_mapWallets);
    {
        BOOST_FOREACH(walletAndMetadata, mapWallets) {
            vIDs.push_back(walletAndMetadata.first);
        }
    }
    return vIDs;
}

void Dealloc()
{
    if (managerSharedInstance)
    {
        UnregisterValidationInterface(managerSharedInstance);
        delete managerSharedInstance;
        managerSharedInstance = NULL;
    }
}

Manager* GetManager()
{
    if (!managerSharedInstance)
    {
        managerSharedInstance = new Manager();
        RegisterValidationInterface(managerSharedInstance);
    }
    return managerSharedInstance;
}

void Manager::SyncTransaction(const CTransaction& tx, const CBlockIndex* pindex, const CBlock* pblock)
{
    LOCK2(cs_main, cs_mapWallets);
    {
        std::pair<std::string, WalletModel> walletAndMetadata;
        BOOST_FOREACH(walletAndMetadata, mapWallets)
        {
            Wallet *pWallet = mapWallets[walletAndMetadata.first].pWallet;
            if (pWallet)
                pWallet->SyncTransaction(tx, pindex, pblock);
        }
    }
}

void Manager::RequestWalletRescan(CBlockIndex* pindexStart, bool fUpdate)
{
    int64_t nNow = GetTime();

    //TODO: do not always scan from the genesis block
    //keep track of the oldest key from all available wallets
    int64_t nTimeFirstKey = 0;

    const CChainParams& chainParams = Params();

    CBlockIndex* pindex = pindexStart;
    {
        LOCK2(cs_main, cs_mapWallets);

        // no need to read and scan block, if block was created before
        // our wallet birthday (as adjusted for block time variability)
        while (pindex && nTimeFirstKey && (pindex->GetBlockTime() < (nTimeFirstKey - 7200)))
            pindex = chainActive.Next(pindex);

        uiInterface.ShowProgress(_("Rescanning..."), 0); // show rescan progress in GUI as dialog or on splashscreen, if -rescan on startup
        double dProgressStart = Checkpoints::GuessVerificationProgress(chainParams.Checkpoints(), pindex, false);
        double dProgressTip = Checkpoints::GuessVerificationProgress(chainParams.Checkpoints(), chainActive.Tip(), false);
        while (pindex)
        {
            if (pindex->nHeight % 100 == 0 && dProgressTip - dProgressStart > 0.0)
                uiInterface.ShowProgress(_("Rescanning..."), std::max(1, std::min(99, (int)((Checkpoints::GuessVerificationProgress(chainParams.Checkpoints(), pindex, false) - dProgressStart) / (dProgressTip - dProgressStart) * 100))));

            CBlock block;
            ReadBlockFromDisk(block, pindex);
            BOOST_FOREACH(CTransaction& tx, block.vtx)
            {
                std::pair<std::string, WalletModel> walletAndMetadata;
                BOOST_FOREACH(walletAndMetadata, mapWallets)
                {
                    Wallet *pWallet = mapWallets[walletAndMetadata.first].pWallet;
                    if (pWallet)
                        pWallet->SyncTransaction(tx, pindex, &block);
                }
            }
            pindex = chainActive.Next(pindex);
            if (GetTime() >= nNow + 60) {
                nNow = GetTime();
                LogPrintf("Still rescanning. At block %d. Progress=%f\n", pindex->nHeight, Checkpoints::GuessVerificationProgress(chainParams.Checkpoints(), pindex));
            }
        }
        uiInterface.ShowProgress(_("Rescanning..."), 100); // hide progress dialog in GUI
    }
}

void Manager::ExecuteRPCI(const std::string& strMethod, const UniValue& params, UniValue& result, bool& accept)
{
    LOCK2(cs_main, cs_mapWallets);
    ExecuteRPC(strMethod, params, result, accept);
}

void GetScriptForMining(boost::shared_ptr<CReserveScript> &script)
{
    //get the default wallet for mining coins
    //TODO: allow user to configure which wallet is used for mining
    Wallet *wallet = CoreWallet::GetManager()->GetWalletWithID("");
    if (wallet)
        wallet->GetScriptForMining(script);
}

void RegisterRPC()
{
    //Extend the existing RPC Server
    //After adding a new endpoint, we can listen to any incomming
    //command over the RPCServer::OnExtendedCommandExecute signal.
    AddJSONRPCURISchema("/corewallet");
    RPCServer::OnExtendedCommandExecute(boost::bind(&Manager::ExecuteRPCI, GetManager(), _1, _2, _3, _4));
}

void RegisterSignals()
{
    RegisterRPC();
    GetMainSignals().ShutdownFinished.connect(boost::bind(&Dealloc));
    GetMainSignals().CreateHelpString.connect(boost::bind(&AppendHelpMessageString, _1, _2));
    GetMainSignals().LoadModules.connect(boost::bind(&LoadAsModule, _1, _2, _3));
    GetMainSignals().ScriptForMining.connect(boost::bind(&GetScriptForMining, _1));
}

void UnregisterSignals()
{
    GetMainSignals().ShutdownFinished.disconnect(boost::bind(&Dealloc));
    GetMainSignals().CreateHelpString.disconnect(boost::bind(&AppendHelpMessageString, _1, _2));
    GetMainSignals().LoadModules.disconnect(boost::bind(&LoadAsModule, _1, _2, _3));
    GetMainSignals().ScriptForMining.disconnect(boost::bind(&GetScriptForMining, _1));
}
};