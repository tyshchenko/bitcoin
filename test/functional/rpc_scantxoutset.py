#!/usr/bin/env python3
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the scantxoutset rpc call."""
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import shutil
import os

class ScantxoutsetTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
    def run_test(self):
        self.log.info("Mining blocks...")
        self.nodes[0].generate(110)

        addr_P2SH_SEGWIT = self.nodes[0].getnewaddress("", "p2sh-segwit")
        pubk1 = self.nodes[0].getaddressinfo(addr_P2SH_SEGWIT)['pubkey']
        addr_LEGACY = self.nodes[0].getnewaddress("", "legacy")
        pubk2 = self.nodes[0].getaddressinfo(addr_LEGACY)['pubkey']
        addr_BECH32 = self.nodes[0].getnewaddress("", "bech32")
        pubk3 = self.nodes[0].getaddressinfo(addr_BECH32)['pubkey']
        self.nodes[0].sendtoaddress(addr_P2SH_SEGWIT, 1)
        self.nodes[0].sendtoaddress(addr_LEGACY, 2)
        self.nodes[0].sendtoaddress(addr_BECH32, 3)
        self.nodes[0].generate(1)

        self.log.info("Stop node, remove wallet, mine again some blocks...")
        self.stop_node(0)
        shutil.rmtree(os.path.join(self.nodes[0].datadir, "regtest", 'wallets'))
        self.start_node(0)
        self.nodes[0].generate(110)

        self.log.info("Send some coins to HD deriven keys")
        #bip32/bip44 tests (use xpriv = "tprv8ZgxMBicQKsPd7Uf69XL1XwhmjHopUGep8GuEiJDZmbQz6o58LninorQAfcKZWARbtRtfnLcJ5MQ2AtHcQJCCRUcMRvmDUjyEmNUWwx8UbK")
        xpub_account_key = "tpubDCWjaJPr3DfCGCvZzKVxXhneVjuRsXkDtoF18BFvci1eeLR97zz8DYr56kgT4sXgYDXQvwQqbvGm6SKRdxd4VLHex3dWXewrgTDZL44UpF5"
        k_bip44_child0_ard = "n1e8DCf7ahpMjVtDvoEfpBwTDEptLxKLs5" #m/44'/0'/0'/0/0
        k_bip44_child1_ard = "n14jYqMLvqG2oJWAr8Fkx2xtxtp8xtqM4C" #m/44'/0'/0'/0/1
        k_bip44_child2_ard = "myRrZdQnkH5aD8hkGX5hXA3S5gL96uszB8" #m/44'/0'/0'/0/2000
        k_bip44_child3_ard = "mnesCcdjvFA5Ys5zQsrLf4B4wmgUwS3kur" #m/44'/0'/0'/1/0
        k_bip44_child4_ard = "mvp9t1iRxGUw8JL615gDsJoq6LzdX2WYRn" #m/44'/0'/0'/1/2000

        self.nodes[0].sendtoaddress(k_bip44_child0_ard, 0.1)
        self.nodes[0].sendtoaddress(k_bip44_child1_ard, 0.2)
        self.nodes[0].sendtoaddress(k_bip44_child2_ard, 0.3)
        self.nodes[0].sendtoaddress(k_bip44_child3_ard, 0.4)
        self.nodes[0].sendtoaddress(k_bip44_child4_ard, 0.5)
        self.nodes[0].generate(1)

        self.stop_node(0)
        os.remove(os.path.join(self.nodes[0].datadir, "regtest", "wallet.dat"))
        self.start_node(0)
        self.log.info("Test if we have found the non HD unspent outputs.")
        assert_equal(self.nodes[0].scantxoutset("start", [ {"pubkey": {"pubkey": pubk1}}, {"pubkey": {"pubkey": pubk2}}, {"pubkey": {"pubkey": pubk3}}])['total_amount'], 6)
        assert_equal(self.nodes[0].scantxoutset("start", [ {"address": addr_P2SH_SEGWIT}, {"address": addr_LEGACY}, {"address": addr_BECH32}])['total_amount'], 6)
        assert_equal(self.nodes[0].scantxoutset("start", [ {"address": addr_P2SH_SEGWIT}, {"address": addr_LEGACY}, {"pubkey": {"pubkey": pubk3}} ])['total_amount'], 6)

        self.log.info("Test invalid parameters.")
        assert_raises_rpc_error(-8, 'Scanobject "pubkey" must contain an object as value', self.nodes[0].scantxoutset, "start", [ {"pubkey": pubk1}]) #missing pubkey object
        assert_raises_rpc_error(-8, 'Scanobject "address" must contain a single string as value', self.nodes[0].scantxoutset, "start", [ {"address": {"address": addr_P2SH_SEGWIT}}]) #invalid object for address
        assert_raises_rpc_error(-8, 'Scanobject "xpub" must contain an object as value', self.nodes[0].scantxoutset, "start", [ {"xpub": pubk1}]) #missing xpub object

        self.log.info("Test if we have found the HD unspent outputs in range 0-1000 (default).")
        assert_equal(self.nodes[0].scantxoutset("start", [ {"xpub": {"xpub": xpub_account_key}}])['total_amount'], Decimal("0.70000000"))
        self.log.info("Ensure that we do find nothing in the unused range 10-20.")
        assert_equal(self.nodes[0].scantxoutset("start", [ {"xpub": {"xpub": xpub_account_key, "range": [10, 20]}}])['total_amount'], 0)
        self.log.info("Ensure we find all HD unspent outputs when using a lookup-window up to key 2000")
        assert_equal(self.nodes[0].scantxoutset("start", [ {"xpub": {"xpub": xpub_account_key, "range": [0, 2000]}}])['total_amount'], Decimal("1.5"))
        self.log.info("Make sure we find all unspent outputs when we search for plain pubkeys AND hd derived keys")
        assert_equal(self.nodes[0].scantxoutset("start", [ {"pubkey": {"pubkey": pubk1}}, {"pubkey": {"pubkey": pubk2}}, {"pubkey": {"pubkey": pubk3}}, {"xpub": {"xpub": xpub_account_key, "range": [0, 2000]}}])['total_amount'], Decimal("7.5"))

if __name__ == '__main__':
    ScantxoutsetTest().main()
