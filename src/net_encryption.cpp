// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <chainparams.h>
#include <crypto/hkdf_sha256_32.h>
#include <net_encryption.h>
#include <net_message.h>

int NetCryptedMessageEnvelope::Read(const char* pch, unsigned bytes)
{
    if (!m_in_data) {
        // copy data to temporary parsing buffer
        unsigned int remaining = m_encryption_handler->GetAADLen() - m_hdr_pos;
        unsigned int copy_bytes = std::min(remaining, bytes);

        memcpy(&vRecv[m_hdr_pos], pch, copy_bytes);
        m_hdr_pos += copy_bytes;

        // if AAD incomplete, exit
        if (m_hdr_pos < m_encryption_handler->GetAADLen()) {
            return copy_bytes;
        }

        // decrypt the length from the AAD
        if (!m_encryption_handler->GetLength(vRecv, m_message_size)) {
            return -1;
        }

        // reject messages larger than MAX_SIZE
        if (m_message_size > MAX_SIZE) {
            return -1;
        }

        // switch state to reading message data
        m_in_data = true;

        return copy_bytes;
    } else {
        // copy the message payload plus the MAC tag
        const unsigned int AAD_LEN = m_encryption_handler->GetAADLen();
        const unsigned int TAG_LEN = m_encryption_handler->GetTagLen();
        unsigned int remaining = m_message_size + TAG_LEN - m_data_pos;
        unsigned int copy_bytes = std::min(remaining, bytes);

        if (vRecv.size() < AAD_LEN + m_data_pos + copy_bytes) {
            // Allocate up to 256 KiB ahead, but never more than the total message size (incl. AAD & TAG).
            vRecv.resize(std::min(AAD_LEN + m_message_size + TAG_LEN, AAD_LEN + m_data_pos + copy_bytes + 256 * 1024 + TAG_LEN));
        }

        memcpy(&vRecv[AAD_LEN + m_data_pos], pch, copy_bytes);
        m_data_pos += copy_bytes;

        return copy_bytes;
    }
}

int NetMessageEncryptionHandshake::Read(const char* pch, unsigned int bytes)
{
    // copy data to temporary parsing buffer
    unsigned int remaining = 32 - m_data_pos;
    unsigned int copy_bytes = std::min(remaining, bytes);
    if (vRecv.size() < 32) {
        vRecv.resize(32);
    }
    memcpy(&vRecv[m_data_pos], pch, copy_bytes);
    m_data_pos += copy_bytes;

    return copy_bytes;
}

bool NetMessageEncryptionHandshake::VerifyHeader() const
{
    CMessageHeader hdr(Params().MessageStart());
    CDataStream str = vRecv; //copy stream to keep function const
    try {
        str >> hdr;
    } catch (const std::exception&) {
        return false;
    }
    if (memcmp(hdr.pchMessageStart, Params().MessageStart(), CMessageHeader::MESSAGE_START_SIZE) == 0 || hdr.GetCommand() == NetMsgType::VERSION) {
        return false;
    }
    return true;
}

int NetCryptedMessage::Read(const char* pch, unsigned int nBytes)
{
    // not supported
    return -1;
}

bool NetCryptedMessage::DecomposeFromStream(CDataStream& stream)
{
    // decompose a single massage from a multimessage envelope
    try {
        stream >> m_command_name;
        stream >> m_message_size;
    } catch (const std::exception&) {
        return false;
    }
    if (m_message_size > MAX_PROTOCOL_MESSAGE_LENGTH || m_message_size > stream.size()) {
        return false;
    }

    // copy the extracted message to the internal message buffer
    if (m_message_size > 0) {
        assert(vRecv.size() == 0);
        vRecv.write(&stream.begin()[0], m_message_size);

        // skip the message payload
        stream.ignore(m_message_size);
    }
    return true;
}

bool BIP151Encryption::AuthenticatedAndDecrypt(CDataStream& data_in_out)
{
    // create a buffer for the decrypted payload
    std::vector<unsigned char> buf_dec;
    buf_dec.resize(data_in_out.size());

    // keep the original payload size
    size_t vsize = data_in_out.size();

    // authenticate and decrypt the message
    LOCK(cs);
    if (chacha20poly1305_crypt(&m_recv_aead_ctx, m_recv_seq_nr++, &buf_dec[0], (const uint8_t*)&data_in_out.data()[0],
            data_in_out.size() - TAG_LEN - AAD_LEN, AAD_LEN, 0) == -1) {
        memory_cleanse(data_in_out.data(), data_in_out.size());
        return false;
    }

    data_in_out.clear();
    data_in_out.write((const char*)&buf_dec.begin()[AAD_LEN], vsize - TAG_LEN - AAD_LEN);
    return true;
}

bool BIP151Encryption::EncryptAppendMAC(std::vector<unsigned char>& data_in_out)
{
    // create a buffer for the encrypted payload
    std::vector<unsigned char> buf_enc;
    buf_enc.resize(data_in_out.size() + TAG_LEN);

    // encrypt and add MAC tag
    LOCK(cs);
    chacha20poly1305_crypt(&m_send_aead_ctx, m_send_seq_nr++, &buf_enc[0], &data_in_out[0],
        data_in_out.size() - AAD_LEN, AAD_LEN, 1);

    // clear data_in and append the decrypted data
    data_in_out.clear();
    data_in_out.insert(data_in_out.begin(), buf_enc.begin(), buf_enc.end());
    return true;
}

bool BIP151Encryption::GetLength(CDataStream& data_in, uint32_t& len_out)
{
    if (data_in.size() < AAD_LEN) {
        return false;
    }

    LOCK(cs);
    if (chacha20poly1305_get_length(&m_recv_aead_ctx, &len_out, m_recv_seq_nr, (const uint8_t*)&data_in.data()[0], AAD_LEN) == -1) {
        return false;
    }

    return true;
}

bool BIP151Encryption::ShouldCryptMsg()
{
    return handshake_done;
}

uint256 BIP151Encryption::GetSessionID()
{
    LOCK(cs);
    return m_session_id;
}

void BIP151Encryption::EnableEncryption(bool inbound)
{
    unsigned char aead_k1[64];
    unsigned char aead_k2[64];

    LOCK(cs);
    if (m_raw_ecdh_secret.size() != 32) {
        return;
    }
    // extract 2 keys for each direction with HKDF HMAC_SHA256 with length 32
    CHKDF_HMAC_SHA256_L32 hkdf_32(&m_raw_ecdh_secret[0], 32, "BitcoinSharedSecret");
    hkdf_32.Expand32("BitcoinK1A", &aead_k1[0]);
    hkdf_32.Expand32("BitcoinK1B", &aead_k1[32]);
    hkdf_32.Expand32("BitcoinK2A", &aead_k2[0]);
    hkdf_32.Expand32("BitcoinK2B", &aead_k2[32]);
    hkdf_32.Expand32("BitcoinSessionID", m_session_id.begin());

    // enabling k1 for send channel on requesting peer and for recv channel on responding peer
    // enabling k2 for recv channel on requesting peer and for send channel on responding peer
    chacha20poly1305_init(&m_send_aead_ctx, inbound ? aead_k2 : aead_k1, sizeof(aead_k1));
    chacha20poly1305_init(&m_recv_aead_ctx, inbound ? aead_k1 : aead_k2, sizeof(aead_k1));

    handshake_done = true;
}

bool BIP151Encryption::GetHandshakeRequestData(std::vector<unsigned char>& handshake_data)
{
    LOCK(cs);
    CPubKey pubkey = m_ecdh_key.GetPubKey();
    m_ecdh_key.VerifyPubKey(pubkey); //verify the pubkey
    assert(pubkey[0] == 2);

    handshake_data.insert(handshake_data.begin(), pubkey.begin() + 1, pubkey.end());
    return true;
}

bool BIP151Encryption::ProcessHandshakeRequestData(const std::vector<unsigned char>& handshake_data)
{
    CPubKey pubkey;
    if (handshake_data.size() != 32) {
        return false;
    }
    std::vector<unsigned char> handshake_data_even_pubkey;
    handshake_data_even_pubkey.push_back(2);
    handshake_data_even_pubkey.insert(handshake_data_even_pubkey.begin() + 1, handshake_data.begin(), handshake_data.end());
    pubkey.Set(handshake_data_even_pubkey.begin(), handshake_data_even_pubkey.end());
    if (!pubkey.IsFullyValid()) {
        return false;
    }

    // calculate ECDH secret
    LOCK(cs);
    bool ret = m_ecdh_key.ComputeECDHSecret(pubkey, m_raw_ecdh_secret);

    // After calculating the ECDH secret, the ephemeral key can be cleansed from memory
    m_ecdh_key.SetNull();
    return ret;
}

BIP151Encryption::BIP151Encryption() : handshake_done(false)
{
    m_ecdh_key.MakeNewKey(true);
    if (m_ecdh_key.GetPubKey()[0] == 3) {
        // the stealth encryption handshake will only use 32byte pubkeys
        // force EVEN (0x02) pubkey be negating the private key in case of ODD (0x03) pubkeys
        m_ecdh_key.Negate();
    }
    assert(m_ecdh_key.IsValid());
}
