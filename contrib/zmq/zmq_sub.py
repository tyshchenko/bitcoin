#!/usr/bin/env python2

import zmq
import binascii

port = 28332
topic1 = "block"
topic2 = "tx"
topic_len = len(topic1)

zmqContext = zmq.Context()
zmqSubSocket = zmqContext.socket(zmq.SUB)
zmqSubSocket.setsockopt(zmq.SUBSCRIBE, topic1)
zmqSubSocket.setsockopt(zmq.SUBSCRIBE, topic2)
zmqSubSocket.connect("tcp://127.0.0.1:%i" % port)


def handleBLK(blk):
    print "-BLKHDR-"
    print binascii.hexlify(blk[:80])


def handleTX(tx):
    print "-TX-"
    print binascii.hexlify(tx)


try:
    while True:
        msg = zmqSubSocket.recv_multipart()
        msg_topic = str(msg[0])
        msg_data  = msg[1]

        if msg_topic == "tx":
            handleTX(msg_data)
        elif msg_topic == "block":
            handleBLK(msg_data)

except KeyboardInterrupt:
    zmqContext.destroy()
