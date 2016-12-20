#!/usr/bin/env python3
# Copyright (c) 2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from pprint import pprint

class AuxiliaryBlockRequestTest (BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 2

    def setup_network(self):
        self.nodes = []
        self.nodes.append(start_node(0, self.options.tmpdir, []))
        #connect to a local machine for debugging
        # url = "http://test:test@%s:%d" % ('127.0.0.1', 18332)
        # proxy = AuthServiceProxy(url)
        # proxy.url = url # store URL on proxy for info
        # self.nodes.append(proxy)
        
        self.nodes.append(start_node(1, self.options.tmpdir, ["-autorequestblocks=0"]))
        connect_nodes_bi(self.nodes, 0, 1)

    def run_test(self):
        print("Mining blocks...")
        self.nodes[0].generate(101)
        self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 2)
        self.nodes[0].generate(1)
        self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 3)
        self.nodes[0].generate(1)
        self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 1)
        time.sleep(5)
        ctps = self.nodes[1].getchaintips()
        pprint(ctps)
        headersheight = -1
        chaintipheight = -1
        for ct in ctps:
            if ct['status'] == "headers-only":
                headersheight = ct['height']
            if ct['status'] == "active":
                chaintipheight = ct['height']
        assert(headersheight == 103)
        assert(chaintipheight == 0)
        node0bbhash = self.nodes[0].getbestblockhash()
        # best block should not be validated, header must be available
        bh = self.nodes[1].getblockheader(node0bbhash, True)
        assert(bh['validated'] == False)
        # block must not be available
        try:
            bh = self.nodes[1].getblock(node0bbhash, True)
            raise AssertionError('Block must not be available')
        except JSONRPCException as e:
            assert(e.error['code']==-32603)

        # request best block (auxiliary)
        self.nodes[1].requestblocks("start", [node0bbhash])
        timeout = 20
        while timeout > 0:
            if self.nodes[1].requestblocks("status")['request_present'] == 0:
                break;
            time.sleep(1)
            timeout-=1
        assert(timeout>0)

        # block must now be available
        block = self.nodes[1].getblock(node0bbhash, True)
        assert(block['hash'] == node0bbhash)
        assert(block['validated'] == False)
        blocks = [node0bbhash]
        lasthash = node0bbhash
        for n in range(0,101):
            bh = self.nodes[1].getblockheader(lasthash)
            blocks.append(bh['hash'])
            lasthash = bh['previousblockhash']
        self.nodes[1].requestblocks("start", blocks[::-1], True)
        timeout = 20
        while timeout > 0:
            if self.nodes[1].requestblocks("status")['request_present'] == 0:
                break;
            time.sleep(1)
            timeout-=1
        assert(timeout>0)
        for bh in blocks:
            block = self.nodes[1].getblock(bh, True)
            assert(block['validated'] == False)
            
        pprint(self.nodes[1].listtransactions())
        self.nodes[0].invalidateblock(node0bbhash)
        self.nodes[0].generate(2)
        time.sleep(5)
        ctps = self.nodes[1].getchaintips()
        bhhash = ""
        pprint(ctps)
        for ct in ctps:
            if ct['status'] == "headers-only" and ct['height'] == 104:
                bhhash = ct['hash']
        self.nodes[1].requestblocks("start", [bhhash], True)
        while timeout > 0:
            if self.nodes[1].requestblocks("status")['request_present'] == 0:
                break;
            time.sleep(1)
            timeout-=1
        assert(timeout>0)
        pprint(self.nodes[1].listtransactions())
                #
        # # enable auto-request of blocks
        # self.nodes[1].setautorequestblocks(True)
        # sync_blocks(self.nodes)
        #
        # ctps = self.nodes[1].getchaintips()
        # # same block must now be available with mode validated=true
        # block = self.nodes[1].getblock(node0bbhash, True)
        # assert(block['hash'] == node0bbhash)
        # assert(block['validated'] == True)
        #
        # chaintipheight = -1
        # for ct in ctps:
        #     if ct['status'] == "active":
        #         chaintipheight = ct['height']
        # assert(chaintipheight == 102)
        
if __name__ == '__main__':
    AuxiliaryBlockRequestTest ().main ()
