#!/usr/bin/env python2
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

class CoreWalletTest (BitcoinTestFramework):

    def setup_chain(self):
        print("Initializing test directory "+self.options.tmpdir)
        initialize_chain_clean(self.options.tmpdir, 4)

    def setup_network(self, split=False):
        

        self.nodes = start_nodes(3, self.options.tmpdir)
        
        #connect to a local machine for debugging
        # url = "http://bitcoinrpc:DP6DvqZtqXarpeNWyN3LZTFchCCyCUuHwNF7E8pX99x1@%s:%d" % ('127.0.0.1', 18332)
        # proxy = AuthServiceProxy(url)
        # proxy.url = url # store URL on proxy for info
        # self.nodes.append(proxy)
        
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.is_network_split=False
        self.sync_all()

    def run_test (self):
        print "Mining blocks..."
        encrypt = True
        self.nodes[0].corewallet.addwallet({"walletid" : "mainwallet"})
        self.nodes[2].corewallet.addwallet({"walletid" : "mainwallet"})
        self.nodes[0].corewallet.hdaddchain()
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        walletinfo = self.nodes[0].corewallet.getbalance({"type" : "all"})
        assert_equal(walletinfo['immature'], 50)
        assert_equal(walletinfo['available'], 0)
        self.sync_all()
        self.nodes[0].generate(100)
        self.sync_all()

        self.nodes[2].corewallet.hdaddchain({"chainpath": "default", "masterprivkey" : 'tprv8ZgxMBicQKsPePWBxbX4F1arnkRyTvM3kVWgGJV2oNJ3abnwgWRhW1q9ruAaW2Y5Ffgak1PRemKd9LgJCrV2vWKeixAvrAAUtyktAMLv4YE'})
        adr = self.nodes[2].corewallet.hdgetaddress()
        assert_equal(adr['address'], "msXnguyqxBFdd7Y2zrsZTU3pKL6fpPCpzX")
        assert_equal(adr['chainpath'], "m/44'/0'/0'/0/0")
        
        self.nodes[2].corewallet.hdaddchain({'chainpath' : "m/101/10'/c", "masterprivkey" : 'tprv8ZgxMBicQKsPfJt4aGm5uB6STj5nCjLCH24rxgnpfusp38cHmcFNoTUan37ndbHCYcQMj544jjNJekSZcET4NoaVGA8s6atuzUHPQBG6mAp'})
        adr = self.nodes[2].corewallet.hdgetaddress()
        assert_equal(adr['address'], "mnvAsVFCiUXh9Sm86JV4EVLfwP9TRz6Yqf")
        assert_equal(adr['chainpath'], "m/101/10'/0/0")

        self.nodes[2].corewallet.hdaddchain({'chainpath' : 'default', "masterprivkey" : 'tprv8ZgxMBicQKsPeoSGUbiFoW2J9Qfz9WnkoT99M4eyoExSi8Pf7qkZ5XrLLRa8s569V8nYnXkL9sAEywJPJ5rxyyHc6QnLiXH9fur4dWcYPTN'})
        adr = self.nodes[2].corewallet.hdgetaddress()
        assert_equal(adr['address'], "n1hBoYyGjqkbC8kdKNAejuaNR19eoYCSoi")
        assert_equal(adr['chainpath'], "m/44'/0'/0'/0/0")
        
        adr2 = self.nodes[2].corewallet.hdgetaddress()
        assert_equal(adr2['address'], "mvFePVSFGELgCDyLYrTJJ3tijnyeB9UF6p")
        assert_equal(adr2['chainpath'], "m/44'/0'/0'/0/1")

        out = self.nodes[0].corewallet.createtx({"sendto": {adr['address'] :  11}, "send" : True})
        
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()
        walletinfo = self.nodes[2].corewallet.getbalance({"type" : "all"})
        assert_equal(walletinfo['available'], 11)

        stop_node(self.nodes[2], 2)
        #bitcoind_processes[2].wait()
        #del bitcoind_processes[2]

        #try to cover over master seed
        os.remove(self.options.tmpdir + "/node2/regtest/mainwallet.cache.logdb")
        os.remove(self.options.tmpdir + "/node2/regtest/mainwallet.private.logdb")
        self.nodes[2] = start_node(2, self.options.tmpdir)

        self.nodes[2].corewallet.hdaddchain({'chainpath' : 'default', "masterprivkey" : 'tprv8ZgxMBicQKsPeoSGUbiFoW2J9Qfz9WnkoT99M4eyoExSi8Pf7qkZ5XrLLRa8s569V8nYnXkL9sAEywJPJ5rxyyHc6QnLiXH9fur4dWcYPTN'})
        #generate address
        adr = self.nodes[2].corewallet.hdgetaddress()
        assert_equal(adr['address'], "n1hBoYyGjqkbC8kdKNAejuaNR19eoYCSoi") #must be deterministic

        walletinfo = self.nodes[2].corewallet.getbalance({"type" : "all"})
        assert_equal(walletinfo['available'], 0) #balance should be o beause we need to rescan first

        stop_node(self.nodes[2], 2)
        self.nodes[2] = start_node(2, self.options.tmpdir, ['-rescan=1']) #do a rescan
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.sync_all()

        walletinfo = self.nodes[2].corewallet.getbalance({"type" : "all"})
        assert_equal(walletinfo['available'], 11) #balance should be o beause we need to rescan first

        balanceOld = self.nodes[0].corewallet.getbalance({"type" : "all"})['available']
        out = self.nodes[2].corewallet.createtx({"sendto": { self.nodes[0].corewallet.hdgetaddress()['address'] :  2.0}, "send" : True})  #try to send (sign) with HD keymaterial
        self.sync_all()

        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        balanceNew = self.nodes[0].corewallet.getbalance({"type" : "all"})['available']
        assert_equal(balanceNew, balanceOld+Decimal('52.00000000'))

        if encrypt:
            print "encrypt wallet"
            self.nodes[2].corewallet.encrypt({"passphrase": "test"})

        errorString = ""
        try:
            out = self.nodes[2].corewallet.createtx({"sendto": { self.nodes[0].corewallet.hdgetaddress()['address'] :  2.0}, "send" : True})
        except JSONRPCException,e:
            errorString = e.error['message']

        assert_equal("Please enter the wallet passphrase" in errorString, True)

        adr = self.nodes[2].corewallet.hdgetaddress()
        assert_equal(adr['chainpath'], "m/44'/0'/0'/0/1") #check if we can create addresses and the counter was increased.

        self.nodes[2].corewallet.unlock({"passphrase": "test", "timeout": 100})

        out = self.nodes[2].corewallet.createtx({"sendto": { self.nodes[0].corewallet.hdgetaddress()['address'] :  2.0}, "send" : True})
        assert_equal(out['sent'], True)

        balanceOld = balanceNew
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()
        balanceNew = self.nodes[0].corewallet.getbalance({"type" : "all"})['available']
        assert_equal(balanceNew, balanceOld+Decimal('52.00000000'))

        newchainofkeys = self.nodes[2].corewallet.hdaddchain()
        adr = self.nodes[2].corewallet.hdgetaddress()
        assert_equal(adr['chainpath'], "m/44'/0'/0'/0/0") #we should start at 0 again
        newAddress = adr['address']
        assert(newAddress != "n1hBoYyGjqkbC8kdKNAejuaNR19eoYCSoi") #should be a different chain

        self.nodes[2].corewallet.hdsetchain({"chainid" : "86ef7ec75970a17abec9bfae7a095798ac71c81eb32af72c5cd4819ac9bff5ba"}) #switch back to the first bip32 chain of keys
        adr = self.nodes[2].corewallet.hdgetaddress({"index": 0}) #get first key in external chain
        assert_equal(adr['address'], "n1hBoYyGjqkbC8kdKNAejuaNR19eoYCSoi")

        self.nodes[2].corewallet.hdsetchain({"chainid" : newchainofkeys["chainid"]}) #switch back to
        adr = self.nodes[2].corewallet.hdgetaddress({"index": 0})
        assert_equal(adr['address'], newAddress)


        ###########################
        # Multiwallet Test        #
        ###########################

        self.nodes[2].corewallet.addwallet({"walletid":"test1"});
        
        #now we have two wallets, need to specify which wallet should be used
        errorString = ""
        try:
            self.nodes[2].corewallet.hdaddchain();
        except JSONRPCException,e:
            errorString = e.error['message']
        assert_equal("not found" in errorString, True)
        
        #test deterministic over multiple wallets
        self.nodes[2].corewallet.hdaddchain({"walletid" : "test1", "masterprivkey": "tprv8ZgxMBicQKsPdt7GUVaSikfckE1ha3qLK6yHuEm4FrcKRwzHNoLHD4DANJGkVqM3MaDhcsVb7dVErZgtYnHXh3Swd5D52JMdD1VEQasS9px"})
        assert_equal(self.nodes[2].corewallet.hdgetaddress({"walletid" : "test1"})['address'], "mwcznB3fc2e3t2yj8msVejtWjgzgpF6isW"); #m/44'/0'/0'/0/0
        assert_equal(self.nodes[2].corewallet.hdgetaddress({"walletid" : "test1"})['address'], "mvnMZ7icbAJDevZBLKip2PG2Pgv7CFNCAL"); #m/44'/0'/0'/0/1
        assert_equal(self.nodes[2].corewallet.hdgetaddress({"walletid" : "test1"})['address'], "mjaFd1QosVabbtBU32vV3QbK3EVgKqajTH"); #m/44'/0'/0'/0/2

        self.nodes[2].corewallet.addwallet({"walletid":"test2"});
        self.nodes[2].corewallet.hdaddchain({"chainpath": "m/45'/1/c", "masterprivkey" : "tprv8ZgxMBicQKsPedkQ9DMiCQpkX7riBxkRwpBDCaXqMDsaeewzeE4EfwN1z7wwuSiSVAbWEEMVydv8ZeNHMf6j18muVe6TYQ9YUSW6sYQH7Ri", "walletid" : "test2"})

        assert_equal(self.nodes[2].corewallet.hdgetaddress({"walletid" : "test2"})['address'], "mrcZEJRs1AVopqk6eybAPqrm5bJXEYFdqd"); #m/45'/1/0/0
        assert_equal(self.nodes[2].corewallet.hdgetaddress({"walletid" : "test2"})['address'], "mvuM5NpPxZtWWjVvr1WRgfqMBMGjPPV5JL"); #m/44'/0'/0'/0/1
        assert_equal(self.nodes[2].corewallet.hdgetaddress({"walletid" : "test2"})['address'], "n39t4aMcSM9Li4WcGRwkmXG2tQc8KLAGSj"); #m/44'/0'/0'/0/2

        balanceOld1 = self.nodes[2].corewallet.getbalance({"walletid": "test1", "type": "all"})
        out = self.nodes[0].corewallet.createtx({"sendto": { self.nodes[2].corewallet.hdgetaddress({"walletid" : "test1", "index": 0})['address'] :  2.0}, "send" : True})
        out = self.nodes[0].corewallet.createtx({"sendto": { self.nodes[2].corewallet.hdgetaddress({"walletid" : "test1", "index": 1})['address'] :  2.0}, "send" : True})
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()
        out = self.nodes[0].corewallet.createtx({"sendto": { self.nodes[2].corewallet.hdgetaddress({"walletid" : "test1", "index": 2})['address'] :  2.0}, "send" : True})
        out = self.nodes[0].corewallet.createtx({"sendto": { self.nodes[2].corewallet.hdgetaddress({"walletid" : "test1", "index": 3})['address'] :  2.1234}, "send" : True})
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()
        assert_equal(balanceOld1['available']+Decimal('8.12340000'), self.nodes[2].corewallet.getbalance({"walletid": "test1", "type": "all"})['available'])
        
        balanceOld2 = self.nodes[2].corewallet.getbalance({"walletid": "test2", "type": "all"})
        out = self.nodes[0].corewallet.createtx({"sendto": { self.nodes[2].corewallet.hdgetaddress({"walletid" : "test2", "index": 0})['address'] :  2.0}, "send" : True})
        out = self.nodes[0].corewallet.createtx({"sendto": { self.nodes[2].corewallet.hdgetaddress({"walletid" : "test2", "index": 1})['address'] :  2.0}, "send" : True})
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()
        out = self.nodes[0].corewallet.createtx({"sendto": { self.nodes[2].corewallet.hdgetaddress({"walletid" : "test2", "index": 2})['address'] :  2.0}, "send" : True})
        out = self.nodes[0].corewallet.createtx({"sendto": { self.nodes[2].corewallet.hdgetaddress({"walletid" : "test2", "index": 3})['address'] :  2.1234}, "send" : True})
        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()
        assert_equal(balanceOld2['available']+Decimal('8.12340000'), self.nodes[2].corewallet.getbalance({"walletid": "test2", "type": "all"})['available'])
        
        #try to recover over master private key in multiwallet mode
        stop_node(self.nodes[2], 2)
        os.remove(self.options.tmpdir + "/node2/regtest/test1.cache.logdb")
        os.remove(self.options.tmpdir + "/node2/regtest/test1.private.logdb")
        os.remove(self.options.tmpdir + "/node2/regtest/test2.cache.logdb")
        os.remove(self.options.tmpdir + "/node2/regtest/test2.private.logdb")
        os.remove(self.options.tmpdir + "/node2/regtest/multiwallet.dat")
        self.nodes[2] = start_node(2, self.options.tmpdir)

        self.nodes[2].corewallet.addwallet({"walletid":"test1"});
        self.nodes[2].corewallet.hdaddchain({"walletid" : "test1", "masterprivkey": "tprv8ZgxMBicQKsPdt7GUVaSikfckE1ha3qLK6yHuEm4FrcKRwzHNoLHD4DANJGkVqM3MaDhcsVb7dVErZgtYnHXh3Swd5D52JMdD1VEQasS9px"})
        self.nodes[2].corewallet.hdgetaddress({"walletid" : "test1"})
        self.nodes[2].corewallet.hdgetaddress({"walletid" : "test1"})
        self.nodes[2].corewallet.hdgetaddress({"walletid" : "test1"})
        self.nodes[2].corewallet.hdgetaddress({"walletid" : "test1"})

        self.nodes[2].corewallet.addwallet({"walletid":"test2"});
        self.nodes[2].corewallet.hdaddchain({"chainpath": "m/45'/1/c", "masterprivkey" : "tprv8ZgxMBicQKsPedkQ9DMiCQpkX7riBxkRwpBDCaXqMDsaeewzeE4EfwN1z7wwuSiSVAbWEEMVydv8ZeNHMf6j18muVe6TYQ9YUSW6sYQH7Ri", "walletid" : "test2"})
        self.nodes[2].corewallet.hdgetaddress({"walletid" : "test2"})
        self.nodes[2].corewallet.hdgetaddress({"walletid" : "test2"})
        self.nodes[2].corewallet.hdgetaddress({"walletid" : "test2"})
        self.nodes[2].corewallet.hdgetaddress({"walletid" : "test2"})
        
        stop_node(self.nodes[2], 2)
        self.nodes[2] = start_node(2, self.options.tmpdir, ['-rescan=1']) #start again with rescan
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        
        assert_equal(balanceOld2['available']+Decimal('8.12340000'), self.nodes[2].corewallet.getbalance({"walletid": "test1", "type": "all"})['available'])
        assert_equal(balanceOld2['available']+Decimal('8.12340000'), self.nodes[2].corewallet.getbalance({"walletid": "test2", "type": "all"})['available'])
        
if __name__ == '__main__':
    CoreWalletTest ().main ()
