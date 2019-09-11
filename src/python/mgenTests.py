from mgenPlayground import mgenEvent as mgenEvent
import mgenPlayground as mgen
from subprocess import call
import time
import sys

def execute_test(event,testname="test",testime=15):

    # log output
    receiver.send_event("output %s-recv.log" %testname)
    sender.send_event("output %s-send.log" %testname)
    sender.send_event("txlog")

    sender.send_event(event.send())
    # turn the flow off after N seconds
    sender.send_event("%d.0 OFF 1" %testime)

    # wait for flow to complete
    time.sleep(testime + 5)
    # run trpr
    call(["trpr","mgen", "input", "%s-recv.log" %testname, "auto", "X", "output", "%s.plot" %testname])

    # popup plot
    call(["gnuplot","-persist", "%s.plot" %testname])

class MyEnum(tuple): __getattr__ = tuple.index
Protocol = MyEnum(["UDP", "TCP", "SINK"])


def main(argv):
    print sys.argv
    global receiver
    global sender

    # Create a mgen receiver
    receiver = mgen.Controller("receiver")
    # Create a mgen sender
    sender = mgen.Controller("sender")

    receiver.send_event("Listen udp 5001")
    receiver.send_event("Listen tcp 5001")

    event = mgenEvent.TxEvent("UDP")
    event.flow_id = 1
    event.pattern = "periodic [-1 8192]"
    #execute_test(event,"speed-test-udp",30)

    event.protocol = getattr(Protocol, "TCP")
    event.flow_id = 2
    #execute_test(event,"speed-test-tcp",30)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
