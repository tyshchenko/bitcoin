// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/legacywallet.h"

#include "main.h"
#include "util.h"
#include "utilmoneystr.h"
#include "validationinterface.h"


#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

CWallet* pwalletMain = NULL;

void CLegacyWallet::RegisterSignals()
{
    GetMainSignals().ShutdownRPCStopped.connect(boost::bind(&CLegacyWallet::Flush, false));
    GetMainSignals().ShutdownNodeStopped.connect(boost::bind(&CLegacyWallet::Flush, true));
    GetMainSignals().ShutdownFinished.connect(boost::bind(&CLegacyWallet::Dealloc));
    GetMainSignals().CreateHelpString.connect(boost::bind(&CLegacyWallet::AppendHelpMessageString, _1, _2));
    GetMainSignals().ParameterInteraction.connect(boost::bind(&CLegacyWallet::MapParameters, _1, _2));
    GetMainSignals().AppInitialization.connect(boost::bind(&CLegacyWallet::SanityCheck, _1));
    GetMainSignals().AppInitializationLogHead.connect(boost::bind(&CLegacyWallet::LogGeneralInfos));
    GetMainSignals().VerifyIntegrity.connect(boost::bind(&CLegacyWallet::Verify, _1, _2, _3));
    GetMainSignals().LoadModules.connect(boost::bind(&CLegacyWallet::LoadAsModule, _1, _2, _3));
    GetMainSignals().NodeStarted.connect(boost::bind(&CLegacyWallet::LogInfos));
    GetMainSignals().FinishInitializing.connect(boost::bind(&CLegacyWallet::StartWalletTasks, _1));
}

void CLegacyWallet::UnregisterSignals()
{
    GetMainSignals().ShutdownRPCStopped.disconnect(boost::bind(&CLegacyWallet::Flush, false));
    GetMainSignals().ShutdownNodeStopped.disconnect(boost::bind(&CLegacyWallet::Flush, true));
    GetMainSignals().ShutdownFinished.disconnect(boost::bind(&CLegacyWallet::Dealloc));
    GetMainSignals().CreateHelpString.disconnect(boost::bind(&CLegacyWallet::AppendHelpMessageString, _1, _2));
    GetMainSignals().ParameterInteraction.disconnect(boost::bind(&CLegacyWallet::MapParameters, _1, _2));
    GetMainSignals().AppInitialization.disconnect(boost::bind(&CLegacyWallet::SanityCheck, _1));
    GetMainSignals().AppInitializationLogHead.disconnect(boost::bind(&CLegacyWallet::LogGeneralInfos));
    GetMainSignals().VerifyIntegrity.disconnect(boost::bind(&CLegacyWallet::Verify, _1, _2, _3));
    GetMainSignals().LoadModules.disconnect(boost::bind(&CLegacyWallet::LoadAsModule, _1, _2, _3));
    GetMainSignals().NodeStarted.disconnect(boost::bind(&CLegacyWallet::LogInfos));
    GetMainSignals().FinishInitializing.disconnect(boost::bind(&CLegacyWallet::StartWalletTasks, _1));
}

void CLegacyWallet::Flush(bool shutdown)
{
    if (pwalletMain)
        pwalletMain->Flush(shutdown);
}

void CLegacyWallet::Dealloc()
{
    delete pwalletMain;
    pwalletMain = NULL;
}

void CLegacyWallet::LogGeneralInfos()
{
    LogPrintf("Using BerkeleyDB version %s\n", DbEnv::version(0, 0, 0));
}

void CLegacyWallet::LogInfos()
{
    LogPrintf("setKeyPool.size() = %u\n",      pwalletMain ? pwalletMain->setKeyPool.size() : 0);
    LogPrintf("mapWallet.size() = %u\n",       pwalletMain ? pwalletMain->mapWallet.size() : 0);
    LogPrintf("mapAddressBook.size() = %u\n",  pwalletMain ? pwalletMain->mapAddressBook.size() : 0);
}

void CLegacyWallet::SanityCheck(std::string& errorString)
{
    std::string strWalletFile = GetWalletFile();
    // Wallet file must be a plain filename without a directory
    if (strWalletFile != boost::filesystem::basename(strWalletFile) + boost::filesystem::extension(strWalletFile))
    errorString += strprintf(_("Wallet %s resides outside data directory"), strWalletFile);
}

bool CLegacyWallet::IsDisabled()
{
    return GetBoolArg("-disablewallet", false);
}

std::string CLegacyWallet::GetWalletFile()
{
    return GetArg("-wallet", "wallet.dat");
}

void CLegacyWallet::LoadAsModule(std::string& warningString, std::string& errorString, bool& stopInit)
{
    if (IsDisabled()) {
        pwalletMain = NULL;
        LogPrintf("Wallet disabled!\n");
    } else {
        uiInterface.InitMessage(_("Loading wallet..."));
        pwalletMain = new CWallet(GetWalletFile());
        
        if (!pwalletMain->LoadWallet(warningString, errorString))
            stopInit = true;
    }
}

void CLegacyWallet::Verify(std::string& warningString, std::string& errorString, bool &stopInit)
{
    if (IsDisabled())
        return;
    
    uiInterface.InitMessage(_("Verifying wallet..."));
    
    const std::string walletFile = GetWalletFile();
    LogPrintf("Using wallet %s\n", walletFile);
    
    if (!pwalletMain->Verify(walletFile, warningString, errorString))
        stopInit = true;
}

void CLegacyWallet::MapParameters(std::string& warningString, std::string& errorString)
{
    if (mapArgs.count("-mintxfee"))
    {
        CAmount n = 0;
        if (ParseMoney(mapArgs["-mintxfee"], n) && n > 0)
        CWallet::minTxFee = CFeeRate(n);
        else
        {
            errorString += strprintf(_("Invalid amount for -mintxfee=<amount>: '%s'"), mapArgs["-mintxfee"]);
            return;
        }
        
    }
    if (mapArgs.count("-paytxfee"))
    {
        CAmount nFeePerK = 0;
        if (!ParseMoney(mapArgs["-paytxfee"], nFeePerK))
        errorString += strprintf(_("Invalid amount for -paytxfee=<amount>: '%s'"), mapArgs["-paytxfee"]); return;
        if (nFeePerK > nHighTransactionFeeWarning)
        warningString += _("Warning: -paytxfee is set very high! This is the transaction fee you will pay if you send a transaction."); return;
        payTxFee = CFeeRate(nFeePerK, 1000);
        if (payTxFee < ::minRelayTxFee)
        errorString += strprintf(_("Invalid amount for -paytxfee=<amount>: '%s' (must be at least %s)"),
                                 mapArgs["-paytxfee"], ::minRelayTxFee.ToString()); return;
    }
    if (mapArgs.count("-maxtxfee"))
    {
        CAmount nMaxFee = 0;
        if (!ParseMoney(mapArgs["-maxtxfee"], nMaxFee))
        errorString += strprintf(_("Invalid amount for -maxtxfee=<amount>: '%s'"), mapArgs["-maptxfee"]); return;
        if (nMaxFee > nHighTransactionMaxFeeWarning)
        warningString += _("Warning: -maxtxfee is set very high! Fees this large could be paid on a single transaction."); return;
        maxTxFee = nMaxFee;
        if (CFeeRate(maxTxFee, 1000) < ::minRelayTxFee)
        errorString += strprintf(_("Invalid amount for -maxtxfee=<amount>: '%s' (must be at least the minrelay fee of %s to prevent stuck transactions)"), mapArgs["-maxtxfee"], ::minRelayTxFee.ToString()); return;
    }
    nTxConfirmTarget = GetArg("-txconfirmtarget", 1);
    bSpendZeroConfChange = GetArg("-spendzeroconfchange", true);
    fSendFreeTransactions = GetArg("-sendfreetransactions", false);
}

void CLegacyWallet::AppendHelpMessageString(std::string& strUsage, bool debugHelp)
{
    if (debugHelp)
    {
        strUsage += HelpMessageOpt("-gen", strprintf(_("Generate coins (default: %u)"), 0));
        strUsage += HelpMessageOpt("-genproclimit=<n>", strprintf(_("Set the number of threads for coin generation if enabled (-1 = all cores, default: %d)"), 1));
        return;
    }
    strUsage += HelpMessageGroup(_("Wallet options:"));
    strUsage += HelpMessageOpt("-disablewallet", _("Do not load the wallet and disable wallet RPC calls"));
    strUsage += HelpMessageOpt("-keypool=<n>", strprintf(_("Set key pool size to <n> (default: %u)"), 100));
    if (GetBoolArg("-help-debug", false))
    strUsage += HelpMessageOpt("-mintxfee=<amt>", strprintf(_("Fees (in BTC/Kb) smaller than this are considered zero fee for transaction creation (default: %s)"),
                                                            FormatMoney(CWallet::minTxFee.GetFeePerK())));
    strUsage += HelpMessageOpt("-paytxfee=<amt>", strprintf(_("Fee (in BTC/kB) to add to transactions you send (default: %s)"), FormatMoney(payTxFee.GetFeePerK())));
    strUsage += HelpMessageOpt("-rescan", _("Rescan the block chain for missing wallet transactions") + " " + _("on startup"));
    strUsage += HelpMessageOpt("-salvagewallet", _("Attempt to recover private keys from a corrupt wallet.dat") + " " + _("on startup"));
    strUsage += HelpMessageOpt("-sendfreetransactions", strprintf(_("Send transactions as zero-fee transactions if possible (default: %u)"), 0));
    strUsage += HelpMessageOpt("-spendzeroconfchange", strprintf(_("Spend unconfirmed change when sending transactions (default: %u)"), 1));
    strUsage += HelpMessageOpt("-txconfirmtarget=<n>", strprintf(_("If paytxfee is not set, include enough fee so transactions begin confirmation on average within n blocks (default: %u)"), 1));
    strUsage += HelpMessageOpt("-maxtxfee=<amt>", strprintf(_("Maximum total fees to use in a single wallet transaction, setting too low may abort large transactions (default: %s)"),
                                                            FormatMoney(maxTxFee)));
    strUsage += HelpMessageOpt("-upgradewallet", _("Upgrade wallet to latest format") + " " + _("on startup"));
    strUsage += HelpMessageOpt("-wallet=<file>", _("Specify wallet file (within data directory)") + " " + strprintf(_("(default: %s)"), "wallet.dat"));
    strUsage += HelpMessageOpt("-walletbroadcast", _("Make the wallet broadcast transactions") + " " + strprintf(_("(default: %u)"), true));
    strUsage += HelpMessageOpt("-walletnotify=<cmd>", _("Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)"));
    strUsage += HelpMessageOpt("-zapwallettxes=<mode>", _("Delete all wallet transactions and only recover those parts of the blockchain through -rescan on startup") +
                               " " + _("(1 = keep tx meta data e.g. account owner and payment request information, 2 = drop tx meta data)"));
}

void CLegacyWallet::StartWalletTasks(boost::thread_group& threadGroup)
{
    if (pwalletMain) {
        // Add wallet transactions that aren't already in a block to mapTransactions
        pwalletMain->ReacceptWalletTransactions();
        
        // Run a thread to flush wallet periodically
        threadGroup.create_thread(boost::bind(&ThreadFlushWalletDB, boost::ref(pwalletMain->strWalletFile)));
    }
}