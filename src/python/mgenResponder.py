import mgen
import struct
import hashlib

import sys

# An array of "Respondents" is built so, given
# a seed, threadID, and messageID, the sender
# can be selected using a random hash.  The array
# is built based on the integer "weight" assigned to each
# respondent.  Heavier "weight" nodes are selected more
# often as the next "respondent".  Note the current implementation
# does not allow a node to respond to itself. If this is desirable,
# it could be added as a configurable option (easy to implement!)

# The weighted responder selection is implemented by
# populating the "pick_array" with nodeIds so that the
# higher weighted nodeIds have correspondingly more 
# entries and higher probability of selection.  This is
# not a very space-efficient approach (as we could do
# with a ProtoSortedTree ;-) but is OK for modest
# node counts and weighting.  
              
class Responder:
    def __init__(self, seed = 0):
        self.seed = int(seed)
        self.resp_dict = {}
        self.pick_array = []
        
    class Respondent:
        def __init__(self, nodeId, addr=None, weight=1):
            self.id = str(nodeId)
            self.addr = addr
            if weight < 1:
                self.weight = 1
            self.weight = int(weight)
        
    def add_respondent(self, nodeId, nodeAddr=None, weight=1):
        r = Responder.Respondent(nodeId, nodeAddr, weight)
        print "adding respondent \"%s\" ..." % r.id
        self.resp_dict[r.id] = r
        
    def get_respondent(self, msgId, prevNodeId = None, threadId = 1):
        if not self.pick_array:
            # pick_array needs to be built (TBD - ProtoTree would be better)
            pickIndex = 0
            for nodeId in sorted(self.resp_dict):
                resp = self.resp_dict[nodeId]
                weight = resp.weight
                resp.pick_index = pickIndex
                for i in range(0, weight):
                    self.pick_array.append(resp)
                pickIndex += weight
        if not self.pick_array:
            return None
        # if prevNodeId, use its index as an offset
        offset = 0
        pickLen = len(self.pick_array)
        if prevNodeId is not None:
            prevResp = self.resp_dict[str(prevNodeId)]
            offset = prevResp.pick_index + prevResp.weight
            pickLen -= prevResp.weight
        # Hash the seed-threadId-msgId tuple into an index to 
        # lookup the respondent
        htext = str(self.seed) + '-' + str(threadId) + '-' + str(msgId)
        index = int(hashlib.md5(htext).hexdigest(), 16)
        if pickLen:
            index = offset + index % pickLen
            index = index % len(self.pick_array)
        else:
            index = 0
        return self.pick_array[index]

    def get_respondent_id(self, msgId, prevNodeId = None, threadId = 1):
        r = self.get_respondent(msgId, prevNodeId, threadId)
        return r.id
        
    def get_respondent_addr(self, msgId, prevNodeId = None, threadId = 1):
        r = self.get_respondent(msgId, prevNodeId, threadId)
        return r.addr
        
# Here are some default config variables

nodeId = None ;# local node id (required to act as a sender/respondent)
threadId = 1

msgSize = 512
msgRate = 1.0

groupAddress = "224.1.2.1"
port = 5001

responder = Responder(5522)

# Usage:  mgenResponder.py id <nodeId> [addr <groupAddr>/<port>][port <port>][unicast][thread <threadId>]
#                          [responders <node1>[/[<addr1>/]<weight1>][,<node2>[/[<addr2>/]<weight2>],...]]
# NOTES:
# 1) If "unicast" is specified, responder list items must give the nodes' unicast addresses
#    (unicast node addresses can be omitted in responder list if using multicast)

i = 1
argc = len(sys.argv)
while i < argc:
    cmd = sys.argv[i]
    if 'seed' == cmd:
        i += 1
        responder.seed = int(sys.argv[i])
    elif 'id' == cmd:
        i += 1
        nodeId = sys.argv[i]
    elif "address".startswith(cmd):
        i += 1
        tmp = sys.argv[1].split('/')
        groupAddress = tmp[0]
        if len(tmp) > 1:
            port = int(tmp[1])
    elif "port".startswith(cmd):
        i += 1
        port = int(sys.argv[i])
    elif "rate".startswith(cmd):
         i += 1
         msgRate = float(sys.argv[i])
    elif "size".startswith(cmd):
         i += 1
         msgRate = float(sys.argv[i])     
    elif "unicast".startswith(cmd):
        groupAddress = None
    elif 'responders'.startswith(cmd):
        i += 1
        nodes = sys.argv[i].split(',')
        for node in nodes:
            tmp = node.split('/')
            name = tmp[0]
            addr = None
            if len(tmp) > 2:
                addr = tmp[1]
                weight = int(tmp[2])
            elif len(tmp) > 1:
                weight = int(tmp[1])
            else:
                weight = 1
            if weight > 0:
                print "adding respondent name:%s addr:%s weight:%d" % (name, addr, weight)
                responder.add_respondent(name, addr, weight)
    else:
        print "mgenResponder error: invalid command: \"%s\"" % cmd
        sys.exit(-1)
    i += 1               
                               
controller = mgen.Controller()

controller.send_command("ipv4")
controller.send_event("listen udp " + str(port))
if groupAddress is not None:
    controller.send_event("join " + groupAddress)

flow = mgen.Flow(1, controller)
flow.set_protocol('udp')
if groupAddress is not None:
    flow.set_destination(groupAddress, port)
flow.set_pattern('periodic [1 %d]' % msgSize)
flow.set_count(1)
flow.set_sequence(1)

if msgRate > 0.0:
    msgDelay = 1.0 / msgRate

payloadStruct = struct.Struct('>II48s') ;# struct format string (integer, integer, 48-byte string)

# By default, we see if it is our job to send message #1
nextSenderId = responder.get_respondent_id(1, None, threadId)

if nextSenderId == nodeId or nextSenderId is None:
    if groupAddress is None:
        # For unicast the IP address of the next respondent (message #2)
        dest = responder.get_respondent_addr(2, nodeId, threadId)
        print "setting unicast dest %s" % dest
        flow.set_destination(dest, port)
    flow.set_struct_payload(payloadStruct, threadId, 1, nodeId)
    print "node %s initiating conversation ..." % nodeId
    flow.start()

for line in controller:
    print line,
    event = mgen.Event(line)
    if mgen.Event.RECV == event.type:
        if event.has_payload():
            (recvThreadId, msgId, senderId) = event.get_struct_payload(payloadStruct)
            senderId = senderId.partition(b'\0')[0] ;# get rid of any null padding the struct string has
            print "recv'd message threadId:%d msgId:%d senderId \"%s\"" % (threadId, msgId, senderId)
            if recvThreadId == threadId:
                msgId += 1
                nextSenderId = responder.get_respondent_id(msgId, senderId, threadId)
                if nextSenderId is None or nextSenderId == nodeId:
                    if flow.active:
                        flow.stop()
                    if groupAddress is None:
                        # For unicast the IP address of the next respondent (message #2)
                        dest = responder.get_respondent_addr(msgId+1, nodeId, threadId)
                        flow.set_destination(dest, port)
                    flow.set_sequence(msgId) ;# make mgen msg sequence match msgId to be cute
                    flow.set_struct_payload(payloadStruct, threadId, msgId, nodeId)
                    flow.start(msgDelay)
            
           
