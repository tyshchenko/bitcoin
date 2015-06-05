#include "zmqpublishnotifier.h"
#include "util.h"

// Internal function to send multipart message
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

bool CZMQAbstractPublishNotifier::Initialize(void *pcontext)
{
    assert(!psocket);
#if ENABLE_ZMQ
    psocket = zmq_socket(pcontext, ZMQ_PUB);
    if (!psocket)
    {
        return false;
    }

    int rc = zmq_bind(psocket, address.c_str());
    return rc == 0;
#else
    return false;
#endif
}

void CZMQAbstractPublishNotifier::Shutdown()
{
    assert(psocket);
#if ENABLE_ZMQ
    // TODO destory socket
    int linger = 0;
    zmq_setsockopt(psocket, ZMQ_LINGER, &linger, sizeof(linger));
    zmq_close(psocket);
#endif
    psocket = 0;
}

void CZMQPublishHashBlockNotifier::NotifyBlock(const uint256 &hash)
{
    int rc = zmq_send_multipart(psocket, "block", 5, hash.begin(), hash.size(), 0);
    LogPrint("zmq", "Publish hash block %s (%d)\n", hash.GetHex().c_str(), rc);
}

void CZMQPublishHashTransactionNotifier::NotifyTransaction(const CTransaction &transaction)
{
    uint256 hash = transaction.GetHash();
    int rc = zmq_send_multipart(psocket, "tx", 2, hash.begin(), hash.size(), 0);
    LogPrint("zmq", "Publish hash transaction %s (%d)\n", hash.GetHex().c_str(), rc);
}
