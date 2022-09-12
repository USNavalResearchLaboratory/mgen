__author__ = 'ljt'

import datetime
import random

class MyEnum(tuple): __getattr__ = tuple.index
Protocol = MyEnum(["UDP", "TCP", "SINK"])
MgenEventType = MyEnum(["ON", "MOD", "OFF", "LISTEN", "IGNORE", "JOIN", "LEAVE"])

class MgenEvent:
    def __init__(self, protocol, flow_id=1):
        self.type = getattr(MgenEventType,"ON")
        self.flow_id = flow_id
        self.tx_time = datetime.time()

      #  if (self.type == 0):
      #      return OnEvent()

class TxEvent(MgenEvent):
    def __init__(self, protocol, flow_id=1, dst_addr="127.0.0.1", dst_port="5000"):
        MgenEvent.__init__(self,protocol)
        self.protocol = getattr(Protocol, protocol)
        self.flow_id = flow_id
        self.dst_addr = dst_addr
        self.dst_port = dst_port
        self.sequence = "-1"
        self.src_port = None
        self.count = -1
        self.pattern = "Periodic [1 1024]"
        self.broadcast = "on"
        self.tos = 0
        self.ttl = 3
        self.interface = "en0"
        self.gps = None
        self.data = None
        self.data_length = None


    def format(self):
        """
       """
        #print "formatting tx event!!"
        cmd = "%s %d" %(MgenEventType[self.type],self.flow_id)


    def send(self):
        self.format()
        cmd = 'ON %s %s DST %s/%s %s' % (
            self.flow_id, Protocol[self.protocol], self.dst_addr, self.dst_port, self.pattern)

        if (self.count > 0):
            cmd = '%s COUNT %s ' % (cmd, self.count)
        if (self.tos > 0):
            cmd = '%s TOS %s ' % (cmd, self.tos)
        if (self.ttl != 3):
            cmd = '%s TTL %s ' % (cmd, self.ttl)
        if (self.data != None):
            cmd = '%s data [%s]' % (cmd, self.data)
        print cmd
        return cmd

class OnEvent(TxEvent):
    def __init__(self):
        TxEvent.__init__(self,"UDP")

    def format(self):
        super.__format(self)
        print "formatting on event!!"

class UDPEvent(TxEvent):
    def __init__(self):
        TxEvent.__init__(self, "UDP")

class TCPEvent(TxEvent):
    def __init__(self):
        TxEvent.__init__(self, "TCP")


class VOIPEvent(TxEvent):
    def __init__(self):
        TxEvent.__init__(self, "UDP")
        # fake flow_id for testing
        self.flow_id = 3
        self.count = -1
        self.pattern = 'BURST [RANDOM 10.0 PERIODIC [10.0 2048] EXP 5.0]'


def ackEvent(sender, event):
    udp = UDPEvent()
    udp.dst_addr = event.src_addr
    udp.count = 1
    udp.flow_id = 4
    sender.send_event(udp.send())


def handlePayload(sender, event):
    print "Handle payload %s" % (event.data)
    behavior_dict = {'01': voip_func,
                     '02': stream_func}

    behavior_func = behavior_dict[event.data]
    behavior_func(sender, event)


def voip_func(sender, event):
    sender.send_event(VOIPEvent().send())
    sender.send_event("%d off 3" % (random.randrange(1, 5)))


def stream_func(sender, event):
    print "Sending stream"
    stream = TCPEvent()
    stream.count = 1
    stream.flow_id = 9
    #print socket.gethostbyname()
    stream.dst_addr = "192.168.1.6"
    stream.dst_port = 5005
    stream.pattern = "PERIODIC [1 3872833]"
    sender.send_event(stream.send())

