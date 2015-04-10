// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LEGACYWALLET_H
#define BITCOIN_LEGACYWALLET_H

#include "wallet/wallet.h"

namespace boost
{
    class thread_group;
} // namespace boos

extern CWallet* pwalletMain;

class CLegacyWallet
{
public:
    static void RegisterSignals();
    static void UnregisterSignals();
    
    static void Flush(bool shutdown=false);
    static void Dealloc();
    
    //! Dump wallet infos to log
    static void LogInfos();
    static void LogGeneralInfos();
    
    //! Verify the wallet database and perform salvage if required
    static void Verify(std::string& warningString, std::string& errorString, bool& stopInit);
    
    static bool IsDisabled();

    //!  */
    static void LoadAsModule(std::string& warningString, std::string& errorString, bool& stopInit);

    //! Map parameters to internal vars
    static void MapParameters(std::string& warningString, std::string& errorString);
    
    static void StartWalletTasks(boost::thread_group& threadGroup);
    
    //! Get user defined wallet file
    static std::string GetWalletFile();
    
    //! Performs sanity check and appends possible errors to given string
    static void SanityCheck(std::string& errorString);
    
    //! append help text to existing string
    static void AppendHelpMessageString(std::string& strUsage, bool debugHelp);
};

#endif // BITCOIN_LEGACYWALLET_H
