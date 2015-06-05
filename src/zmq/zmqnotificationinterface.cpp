// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zmqnotificationinterface.h"
#include "zmqpublishnotifier.h"

#include "version.h"
#include "main.h"
#include "streams.h"
#include "util.h"
#include "netbase.h"

void zmqError(const char *str)
{
#if ENABLE_ZMQ
    LogPrint("zmq", "Error: %s, errno=%s\n", str, zmq_strerror(errno));
#endif
}

CZMQNotificationInterface::CZMQNotificationInterface() : pcontext(NULL)
{
}

CZMQNotificationInterface::~CZMQNotificationInterface()
{
    // ensure Shutdown if Initialize is called
    assert(!pcontext);

    for (std::list<CZMQAbstractNotifier*>::iterator i=notifiers.begin(); i!=notifiers.end(); ++i)
    {
        delete *i;
    }
}

CZMQNotificationInterface* CZMQNotificationInterface::CreateWithArguments(const std::map<std::string, std::string> &args)
{
    CZMQNotificationInterface* notificationInterface = 0;
    std::map<std::string, CZMQNotifierFactory> factories;
    std::list<CZMQAbstractNotifier*> notifiers;

    factories["-zmqpubhashblock"] = CZMQAbstractNotifier::Create<CZMQPublishHashBlockNotifier>;
    factories["-zmqpubhashtransaction"] = CZMQAbstractNotifier::Create<CZMQPublishHashTransactionNotifier>;

    for (std::map<std::string, CZMQNotifierFactory>::const_iterator i=factories.begin(); i!=factories.end(); ++i)
    {
        std::map<std::string, std::string>::const_iterator j = args.find(i->first);
        if (j!=args.end())
        {
            CZMQNotifierFactory factory = i->second;
            std::string address = j->second;
            CZMQAbstractNotifier *notifier = factory();
            notifier->SetAddress(address);
            notifiers.push_back(notifier);
        }
    }

    if (!notifiers.empty())
    {
        notificationInterface = new CZMQNotificationInterface();
        notificationInterface->notifiers = notifiers;
    }

    return notificationInterface;
}

// Called at startup to conditionally set up ZMQ socket(s)
bool CZMQNotificationInterface::Initialize()
{
    LogPrint("zmq", "Initialize notification interface\n");
    assert(!pcontext);

#if ENABLE_ZMQ
    pcontext = zmq_init(1);
#endif
    if (!pcontext)
    {
        zmqError("Unable to initialize context");
        return false;
    }

    std::list<CZMQAbstractNotifier*>::iterator i=notifiers.begin();
    for (; i!=notifiers.end(); ++i)
    {
        CZMQAbstractNotifier *notifier = *i;
        if (notifier->Initialize(pcontext))
        {
            LogPrint("zmq", "  Notifier ready %s\n", notifier->GetAddress().c_str());
        }
        else
        {
            LogPrint("zmq", "  Notifier failed %s\n", notifier->GetAddress().c_str());
            break;
        }
    }

    if (i!=notifiers.end())
    {
        Shutdown();
        return false;
    }

    return false;
}

// Called during shutdown sequence
void CZMQNotificationInterface::Shutdown()
{
    LogPrint("zmq", "Shutdown notification interface\n");
    if (pcontext)
    {
        for (std::list<CZMQAbstractNotifier*>::iterator i=notifiers.begin(); i!=notifiers.end(); ++i)
        {
            CZMQAbstractNotifier *notifier = *i;
            notifier->Shutdown();
        }
#if ENABLE_ZMQ
        zmq_ctx_destroy(pcontext);
#endif
        pcontext = 0;
    }
}

void CZMQNotificationInterface::UpdatedBlockTip(const uint256 &hash)
{
    for (std::list<CZMQAbstractNotifier*>::iterator i = notifiers.begin(); i!=notifiers.end(); ++i)
    {
        CZMQAbstractNotifier *notifier = *i;
        notifier->NotifyBlock(hash);
    }
}

void CZMQNotificationInterface::SyncTransaction(const CTransaction &tx, const CBlock *pblock)
{
    for (std::list<CZMQAbstractNotifier*>::iterator i = notifiers.begin(); i!=notifiers.end(); ++i)
    {
        CZMQAbstractNotifier *notifier = *i;
        notifier->NotifyTransaction(tx);
    }
}
