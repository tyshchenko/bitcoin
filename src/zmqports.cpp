// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zmqports.h"

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "primitives/block.h"
#include "primitives/transaction.h"
#include "version.h"
#include "main.h"
#include "streams.h"
#include "util.h"
#include "netbase.h"

#if ENABLE_ZMQ
#include <zmq.h>
#include <stdarg.h>

// Internal utility functions
static void zmqError(const char *str)
{
    LogPrint("zmq", "Error: %s, errno=%s\n", str, zmq_strerror(errno));
}
#endif

CZMQPublisher::CZMQPublisher()
    : pcontext(NULL)
    , psocket(NULL)
    , format(HashFormat)
{
}

CZMQPublisher::~CZMQPublisher()
{
    // ensure Shutdown if Initialize is called
    assert(!pcontext);
    assert(!psocket);
}

// Called at startup to conditionally set up ZMQ socket(s)
void CZMQPublisher::Initialize(const std::string &endpoint, CZMQPublisher::Format fmt)
{
#if ENABLE_ZMQ
    assert(!pcontext);
    assert(!psocket);

    pcontext = zmq_init(1);
    if (!pcontext)
    {
        zmqError("Unable to initialize context");
        return;
    }

    psocket = zmq_socket(pcontext, ZMQ_PUB);
    if (!psocket)
    {
        zmqError("Unable to open pub socket");
        zmq_ctx_destroy(pcontext);
        pcontext = 0;
        return;
    }

    int rc = zmq_bind(psocket, endpoint.c_str());
    if (rc != 0)
    {
        zmqError("Unable to bind socket");
        zmq_close(psocket);
        zmq_ctx_destroy(pcontext);
        psocket = 0;
        pcontext = 0;
        return;
    }

    format = fmt;
    LogPrint("zmq", "PUB socket listening at %s\n", endpoint);
#endif
}

// Called during shutdown sequence
void CZMQPublisher::Shutdown()
{
#if ENABLE_ZMQ
    if(psocket)
    {
        int linger = 0;
        zmq_setsockopt(psocket, ZMQ_LINGER, &linger, sizeof(linger));
        zmq_close(psocket);
        psocket = 0;

        zmq_ctx_destroy(pcontext);
        pcontext = 0;
    }
#endif
}


// Internal function to send amultipart pack a zmq_msg_t and send it
static int zmq_send_multipart(void *sock, const void* data, size_t size, ...)
{
#if ENABLE_ZMQ
    va_list args;
    va_start(args, size);

    while (1)
    {
        zmq_msg_t msg;

        int rc = zmq_msg_init_size(&msg, size);
        if (rc != 0)
        {
            zmqError("Unable to initialize ZMQ msg");
            return -1;
        }

        void *buf = zmq_msg_data(&msg);
        memcpy(buf, data, size);

        data = va_arg(args, const void*);

        rc = zmq_msg_send(&msg, sock, data ? ZMQ_SNDMORE : 0);
        if (rc == -1)
        {
            zmqError("Unable to send ZMQ msg");
            zmq_msg_close(&msg);
            return -1;
        }

        zmq_msg_close(&msg);

        if (!data)
            break;

        size = va_arg(args, size_t);
    }
#endif
    return 0;
}

void CZMQPublisher::SyncTransaction(const CTransaction &tx, const CBlock *pblock)
{
#if ENABLE_ZMQ
    if (!psocket)
        return;

    int rc = 0;

    if (format==HashFormat)
    {
        uint256 hash = tx.GetHash();
        rc = zmq_send_multipart(psocket, "TXN", 3, hash.begin(), hash.size(), 0);
    }
    else if (format==NetworkFormat)
    {
        // Serialize transaction
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << tx;
        rc = zmq_send_multipart(psocket, "TXN", 3, &ss[0], ss.size(), 0);
    }

    if (rc!=0)
        zmqError("Unable to send multipart message");
#endif
}

// Called after the block chain tip changed
void CZMQPublisher::UpdatedBlockTip(const uint256 &hash)
{
#if ENABLE_ZMQ
    if (!psocket)
        return;

    int rc = 0;

    if (format==HashFormat)
    {
        zmq_send_multipart(psocket, "BLK", 3, hash.begin(), hash.size(), 0);
    }
    else if (format==NetworkFormat)
    {
        CBlock block;
        {
            LOCK(cs_main);
            CBlockIndex* pblockindex = mapBlockIndex[hash];
            if(!ReadBlockFromDisk(block, pblockindex))
                return;
        }

        // Serialize block
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << block;
        rc = zmq_send_multipart(psocket, "BLK", 3, &ss[0], ss.size(), 0);
    }

    if (rc!=0)
        zmqError("Unable to send multipart message");
#endif
}

