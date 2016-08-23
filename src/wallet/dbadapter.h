// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_DBADABTER_H
#define BITCOIN_WALLET_DBADABTER_H

#include "clientversion.h"
#include "serialize.h"
#include "streams.h"

typedef void DBAstractCursor;
class CWalletDBAdapterAbstract
{
public:
    static CWalletDBAdapterAbstract* initWithType(const std::string& strFilename, const char* pszMode = "r+", bool fFlushOnCloseIn=true);

    virtual ~CWalletDBAdapterAbstract() {}

    virtual bool ReadS(CDataStream& ssKey, CDataStream& ssValue) = 0;
    virtual bool WriteS(CDataStream& ssKey, CDataStream& ssValue, bool fOverwrite = true) = 0;
    virtual bool EraseS(CDataStream& ssKey) = 0;
    virtual bool ExistsS(CDataStream& ssKey) = 0;

    virtual DBAstractCursor* GetCursor() = 0;
    virtual int ReadAtCursor(DBAstractCursor* pcursor, CDataStream& ssKey, CDataStream& ssValue, bool setRange = false) = 0;
    virtual void CloseCursor(DBAstractCursor* pcursor) = 0;

    virtual bool TxnBegin() = 0;
    virtual bool TxnCommit() = 0;
    virtual bool TxnAbort() = 0;
    virtual bool WriteVersion(int nVersion) = 0;

    template <typename K, typename T>
    bool Read(const K& key, T& value)
    {
        // Key
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);

        bool suc = ReadS(ssKey, ssValue);
        if (suc)
            ssValue >> value;
        return suc;
    }

    template <typename K, typename T>
    bool Write(const K& key, const T& value, bool fOverwrite = true)
    {
        // Key
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;


        // Value
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.reserve(10000);
        ssValue << value;

        return WriteS(ssKey, ssValue, fOverwrite);
    }

    template <typename K>
    bool Erase(const K& key)
    {
        // Key
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;

        return EraseS(ssKey);
    }

    template <typename K>
    bool Exists(const K& key)
    {
        // Key
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;

        return ExistsS(ssKey);
    }
};


#endif // BITCOIN_WALLET_DBADABTER_H
