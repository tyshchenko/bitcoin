// Copyright (c) 2012-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "addrman.h"
#include "test/test_bitcoin.h"
#include <string>
#include <boost/test/unit_test.hpp>
#include "hash.h"
#include "serialize.h"
#include "streams.h"
#include "net.h"
#include "netbase.h"
#include "chainparams.h"

using namespace std;


BOOST_FIXTURE_TEST_SUITE(net_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(cnode_simple_test)
{
    SOCKET hSocket = INVALID_SOCKET;

    in_addr ipv4Addr;
    ipv4Addr.s_addr = 0xa0b0c001;
    
    CAddress addr = CAddress(CService(ipv4Addr, 7777), NODE_NETWORK);
    std::string pszDest = "";
    bool fInboundIn = false;

    // Test that fFeeler is false by default.
    CNode* pnode1 = new CNode(hSocket, addr, pszDest, fInboundIn);
    BOOST_CHECK(pnode1->fInbound == false);
    BOOST_CHECK(pnode1->fFeeler == false);

    fInboundIn = true;
    CNode* pnode2 = new CNode(hSocket, addr, pszDest, fInboundIn);
    BOOST_CHECK(pnode2->fInbound == true);
    BOOST_CHECK(pnode2->fFeeler == false);
}

BOOST_AUTO_TEST_SUITE_END()
