// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/db.h"
#include "wallet/dbadapter.h"

CWalletDBAdapterAbstract* CWalletDBAdapterAbstract::initWithType(const std::string& strFilename, const char* pszMode, bool fFlushOnCloseIn)
{
    return new CDB(strFilename, pszMode, fFlushOnCloseIn);
}