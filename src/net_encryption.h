// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NET_ENCRYPTION_H
#define BITCOIN_NET_ENCRYPTION_H

#include <memory>

#include <key.h>
#include <net_message.h>
#include <streams.h>
#include <uint256.h>


class EncryptionHandlerInterface
{
public:
    virtual ~EncryptionHandlerInterface() {}

    virtual bool GetLength(CDataStream& data_in, uint32_t& len_out) = 0;
    virtual bool EncryptAppendMAC(std::vector<unsigned char>& data_in_out) = 0;
    virtual bool AuthenticatedAndDecrypt(CDataStream& data_in_out) = 0;
    virtual bool ShouldCryptMsg() = 0;

    virtual unsigned int GetTagLen() const = 0;
    virtual unsigned int GetAADLen() const = 0;
};

class BIP151Encryption : public EncryptionHandlerInterface
{
private:
    static constexpr unsigned int TAG_LEN = 16; /* poly1305 128bit MAC tag */
    static constexpr unsigned int AAD_LEN = 4;  /* uint32 payload length */

public:
    bool GetLength(CDataStream& data_in, uint32_t& len_out) override;
    bool EncryptAppendMAC(std::vector<unsigned char>& data_in_out) override;
    bool AuthenticatedAndDecrypt(CDataStream& data_in_out) override;

    bool ShouldCryptMsg() override;

    unsigned inline int GetTagLen() const override
    {
        return TAG_LEN;
    }

    unsigned inline int GetAADLen() const override
    {
        return AAD_LEN;
    }
};
typedef std::shared_ptr<EncryptionHandlerInterface> EncryptionHandlerRef;

//encrypted network message envelope after BIP151
class NetCryptedMessageEnvelope : public NetMessageBase
{
public:
    bool m_in_data;
    uint32_t m_message_size;
    unsigned int m_hdr_pos;
    uint32_t m_data_pos;

    EncryptionHandlerRef m_encryption_handler;

    NetCryptedMessageEnvelope(EncryptionHandlerRef encryption_handler, const CMessageHeader::MessageStartChars& pchMessageStartIn, int nTypeIn, int nVersionIn)
        : NetMessageBase(nTypeIn, nVersionIn),
          m_encryption_handler(encryption_handler)
    {
        // resize the message buffer to the AADlen (ex. 4 byte/uint32_t for BIP151)
        vRecv.resize(m_encryption_handler->GetAADLen());
        m_message_size = 0;
        m_hdr_pos = 0;
        m_data_pos = 0;
        m_in_data = 0;
        m_type = NetMessageType::BIP151_ENVELOPE;
    }

    bool Complete() const override
    {
        if (!m_in_data) {
            return false;
        }
        return (m_message_size + m_encryption_handler->GetTagLen() == m_data_pos);
    }

    uint32_t GetMessageSize() const override
    {
        return m_message_size;
    }

    uint32_t GetMessageSizeWithHeader() const override
    {
        return m_message_size + sizeof(m_message_size) + m_encryption_handler->GetAADLen() /*tag*/;
    }

    std::string GetCommandName() const override
    {
        return ""; //BIP151 envelope, no message name
    }

    const uint256& GetMessageHash() const;

    int Read(const char* pch, unsigned int nBytes) override;

    bool VerifyMessageStart() const override { return true; }
    bool VerifyHeader() const override { return true; }
    bool VerifyChecksum(std::string& error) const override { return true; }
};

//encrypted network message (inner message) after BIP151
class NetCryptedMessage : public NetMessageBase
{
public:
    uint32_t m_message_size;
    std::string m_command_name;
    int m_read_type;

    NetCryptedMessage(const CMessageHeader::MessageStartChars& pchMessageStartIn, int nTypeIn, int nVersionIn) : NetMessageBase(nTypeIn, nVersionIn)
    {
        m_message_size = 0;
        m_command_name.clear();
        m_type = NetMessageType::BIP151_MSG;
    }

    bool Complete() const override
    {
        return true;
    }

    uint32_t GetMessageSize() const override
    {
        return m_message_size;
    }

    uint32_t GetMessageSizeWithHeader() const override
    {
        return m_message_size + sizeof(m_message_size) + GetSerializeSize(m_command_name, SER_NETWORK, PROTOCOL_VERSION);
    }

    std::string GetCommandName() const override
    {
        return m_command_name;
    }

    int Read(const char* pch, unsigned int nBytes) override;
    bool DecomposeFromStream(CDataStream& stream);

    bool VerifyMessageStart() const override { return true; }
    bool VerifyHeader() const override { return true; }
    bool VerifyChecksum(std::string& error) const override { return true; }
};

typedef std::unique_ptr<NetCryptedMessage> NetCryptedMessageRef;

#endif // BITCOIN_NET_ENCRYPTION_H
