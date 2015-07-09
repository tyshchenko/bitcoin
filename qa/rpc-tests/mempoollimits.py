#!/usr/bin/env python2
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test node handling
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *
from pprint import pprint
import base64

try:
    import http.client as httplib
except ImportError:
    import httplib
try:
    import urllib.parse as urlparse
except ImportError:
    import urlparse

class NodeHandlingTest (BitcoinTestFramework):
    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir, extra_args=[
            ['-debug=mempool', '-janitorinterval=5', '-mempoolhighwater=6000', '-mempoollowwater=3000'],
            ['-debug=mempool', '-janitorinterval=5', '-mempoolhighwater=6000', '-mempoollowwater=3000'],
            ['-debug=mempool', '-janitorinterval=5', '-mempoolhighwater=6000', '-mempoollowwater=3000'],
            ['-debug=mempool', '-janitorinterval=5', '-mempoolhighwater=6000', '-mempoollowwater=3000']
        ])
    
    
    def run_test(self):
        pprint(self.nodes[0].getmempoolinfo())
        
        for x in range(0,10000):
            self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 0.1)
            if x % 10:
                pprint(self.nodes[0].getmempoolinfo())
                pprint(self.nodes[1].getnetworkinfo())
                print "==--=="
                
            
        print "===="
        pprint(self.nodes[0].getmempoolinfo())
        self.sync_all()
        pprint(self.nodes[1].getmempoolinfo())
        print "----"
        pprint(self.nodes[1].getnetworkinfo())
        

if __name__ == '__main__':
    NodeHandlingTest ().main ()
