// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "corewallet/hdkeystore.h"

#include "base58.h"
#include "util.h"
#include "utilstrencodings.h"

#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

namespace CoreWallet {
bool CHDKeyStore::AddExtendedMasterKey(const HDChainID& hash, const CExtKey& extKeyIn)
{
    LOCK(cs_KeyStore);
    if (IsCrypted())
    {
        std::vector<unsigned char> vchCryptedSecret;
        if (!CCryptoKeyStore::EncryptExtendedMasterKey(extKeyIn, hash, vchCryptedSecret))
            return false;

        mapHDCryptedExtendedMasterKeys[hash] = vchCryptedSecret;
        return true;
    }
    mapHDMasterExtendedMasterKey[hash] = extKeyIn;
    return true;
}

bool CHDKeyStore::AddCryptedExtendedMasterKey(const HDChainID& hash, const std::vector<unsigned char>& vchCryptedSecret)
{
    LOCK(cs_KeyStore);
    if (!SetCrypted())
        return false;
    mapHDCryptedExtendedMasterKeys[hash] = vchCryptedSecret;
    return true;
}

bool CHDKeyStore::GetExtendedMasterKey(const HDChainID& hash, CExtKey& extKeyOut) const
{
    LOCK(cs_KeyStore);
    if (!IsCrypted())
    {
        std::map<HDChainID, CExtKey >::const_iterator it=mapHDMasterExtendedMasterKey.find(hash);
        if (it == mapHDMasterExtendedMasterKey.end())
            return false;

        extKeyOut = it->second;
        return true;
    }
    else
    {
        std::map<HDChainID, std::vector<unsigned char> >::const_iterator it=mapHDCryptedExtendedMasterKeys.find(hash);
        if (it == mapHDCryptedExtendedMasterKeys.end())
            return false;

        std::vector<unsigned char> vchCryptedSecret = it->second;
        if (!DecryptExtendedMasterKey(vchCryptedSecret, hash, extKeyOut))
            return false;

        return true;
    }
    return false;
}

bool CHDKeyStore::EncryptExtendedMasterKey()
{
    LOCK(cs_KeyStore);
    for (std::map<HDChainID, CExtKey>::iterator it = mapHDMasterExtendedMasterKey.begin(); it != mapHDMasterExtendedMasterKey.end(); ++it)
    {
        std::vector<unsigned char> vchCryptedSecret;
        if (!CCryptoKeyStore::EncryptExtendedMasterKey(it->second, it->first, vchCryptedSecret))
            return false;
        AddCryptedExtendedMasterKey(it->first, vchCryptedSecret);
    }
    mapHDMasterExtendedMasterKey.clear();
    return true;
}

bool CHDKeyStore::GetCryptedExtendedMasterKey(const HDChainID& hash, std::vector<unsigned char>& vchCryptedSecret) const
{
    LOCK(cs_KeyStore);
    if (!IsCrypted())
        return false;

    std::map<HDChainID, std::vector<unsigned char> >::const_iterator it=mapHDCryptedExtendedMasterKeys.find(hash);
    if (it == mapHDCryptedExtendedMasterKeys.end())
        return false;

    vchCryptedSecret = it->second;
    return true;
}

bool CHDKeyStore::HaveKey(const CKeyID &address) const
{
    LOCK(cs_KeyStore);
    if (mapHDPubKeys.count(address) > 0)
        return true;

    return CCryptoKeyStore::HaveKey(address);
}

bool CHDKeyStore::LoadHDPubKey(const CHDPubKey &pubkey)
{
    LOCK(cs_KeyStore);
    mapHDPubKeys[pubkey.pubkey.GetID()] = pubkey;
    return true;
}

bool CHDKeyStore::GetHDPubKey(const CKeyID &address, CHDPubKey &pubkeyOut) const
{
    LOCK(cs_KeyStore);
    if (mapHDPubKeys.count(address) > 0)
    {
        std::map<CKeyID, CHDPubKey>::const_iterator mi = mapHDPubKeys.find(address);
        if (mi != mapHDPubKeys.end())
        {
            pubkeyOut = mi->second;
            return true;
        }
    }
    return false;
}

bool CHDKeyStore::GetAvailableChainIDs(std::vector<HDChainID>& chainIDs)
{
    LOCK(cs_KeyStore);
    chainIDs.clear();

    if (IsCrypted())
    {
        for (std::map<HDChainID, std::vector<unsigned char> >::iterator it = mapHDCryptedExtendedMasterKeys.begin(); it != mapHDCryptedExtendedMasterKeys.end(); ++it) {
            chainIDs.push_back(it->first);
        }
    }
    else
    {
        for (std::map<HDChainID, CExtKey >::iterator it = mapHDMasterExtendedMasterKey.begin(); it != mapHDMasterExtendedMasterKey.end(); ++it) {
            chainIDs.push_back(it->first);
        }
    }
    
    return true;
}

bool CHDKeyStore::GetKey(const CKeyID &address, CKey &keyOut) const
{
    LOCK(cs_KeyStore);

    std::map<CKeyID, CHDPubKey>::const_iterator mi = mapHDPubKeys.find(address);
    if (mi != mapHDPubKeys.end())
    {
        if (!DeriveKey(mi->second, keyOut))
            return false;

        return true;
    }

    return CCryptoKeyStore::GetKey(address, keyOut);
}

bool CHDKeyStore::GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    LOCK(cs_KeyStore);

    std::map<CKeyID, CHDPubKey>::const_iterator mi = mapHDPubKeys.find(address);
    if (mi != mapHDPubKeys.end())
    {
        vchPubKeyOut = mi->second.pubkey;
        return true;
    }

    return CCryptoKeyStore::GetPubKey(address, vchPubKeyOut);
}

bool CHDKeyStore::DeriveKey(const CHDPubKey hdPubKey, CKey& keyOut) const
{
    //this methode required no locking
    
    std::string chainPath = hdPubKey.chainPath;
    std::vector<std::string> pathFragments;
    boost::split(pathFragments, chainPath, boost::is_any_of("/"));

    LogPrint("hdwallet", "derive key %s\n", chainPath);
    CExtKey extKey;
    CExtKey parentKey;
    BOOST_FOREACH(std::string fragment, pathFragments)
    {
        bool harden = false;
        if (*fragment.rbegin() == '\'')
        {
            harden = true;
            fragment = fragment.substr(0,fragment.size()-1);
        }

        if (fragment == "m")
        {
            CExtKey bip32MasterKey;

            // get master seed
            if (!GetExtendedMasterKey(hdPubKey.chainHash, bip32MasterKey))
                return false;

            parentKey = bip32MasterKey;
        }
        else if (fragment == "c")
        {
            return false;
        }
        else
        {
            CExtKey childKey;
            int32_t nIndex;
            if (!ParseInt32(fragment,&nIndex))
                return false;
            parentKey.Derive(childKey, (harden ? 0x80000000 : 0)+nIndex);
            parentKey = childKey;
        }
    }
    keyOut = parentKey.key;
    LogPrint("hdwallet", "derived key with adr: %s\n", CBitcoinAddress(keyOut.GetPubKey().GetID()).ToString());
    return true;
}

bool CHDKeyStore::DeriveHDPubKeyAtIndex(const HDChainID chainId, CHDPubKey& hdPubKeyOut, unsigned int nIndex, bool internal) const
{
    CHDChain hdChain;
    if (!GetChain(chainId, hdChain))
        return false;

    if ( (internal && !hdChain.internalPubKey.pubkey.IsValid()) || !hdChain.externalPubKey.pubkey.IsValid())
        throw std::runtime_error("CHDKeyStore::HDGetChildPubKeyAtIndex(): Missing HD extended pubkey!");

    if (nIndex >= 0x80000000)
        throw std::runtime_error("CHDKeyStore::HDGetChildPubKeyAtIndex(): No more available keys!");

    CExtPubKey useExtKey = internal ? hdChain.internalPubKey : hdChain.externalPubKey;
    CExtPubKey childKey;
    if (!useExtKey.Derive(childKey, nIndex))
        throw std::runtime_error("CHDKeyStore::HDGetChildPubKeyAtIndex(): Key deriving failed!");

    hdPubKeyOut.pubkey = childKey.pubkey;
    hdPubKeyOut.chainHash = chainId;
    hdPubKeyOut.nChild = nIndex;
    hdPubKeyOut.chainPath = hdChain.chainPath;
    hdPubKeyOut.internal = internal;
    boost::replace_all(hdPubKeyOut.chainPath, "c", itostr(internal)); //replace the chain switch index
    hdPubKeyOut.chainPath += "/"+itostr(nIndex);

    return true;
}

unsigned int CHDKeyStore::GetNextChildIndex(const HDChainID& chainId, bool internal)
{
    std::vector<unsigned int> vIndices;

    {
        LOCK(cs_KeyStore);
        //get next unused child index
        for (std::map<CKeyID, CHDPubKey>::iterator it = mapHDPubKeys.begin(); it != mapHDPubKeys.end(); ++it)
            if (it->second.chainHash == chainId && it->second.internal == internal)
                vIndices.push_back(it->second.nChild);
    }

    for (unsigned int i=0;i<0x80000000;i++)
        if (std::find(vIndices.begin(), vIndices.end(), i) == vIndices.end())
            return i;

    return 0;
}

bool CHDKeyStore::AddChain(const CHDChain& chain)
{
    LOCK(cs_KeyStore);
    mapChains[chain.chainHash] = chain;
    return true;
}

bool CHDKeyStore::GetChain(const HDChainID chainId, CHDChain& chainOut) const
{
    LOCK(cs_KeyStore);
    std::map<HDChainID, CHDChain>::const_iterator it=mapChains.find(chainId);
    if (it == mapChains.end())
        return false;

    chainOut = it->second;
    return true;
}
}; //end namespace