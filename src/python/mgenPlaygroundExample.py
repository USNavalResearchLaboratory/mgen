from mgenPlayground import mgenEvent as mgenEvent
#import mgenPlayground.mgenEvent as mgenEvent
import mgenPlayground as mgen
import random

# Create an MgenController instance
# (an underlying "mgen" process is instantiated)
receiver = mgen.Controller()

# Send a global command
receiver.send_command("ipv4")

# Send a script event to enact immediately (mgen udp receiver)
receiver.send_event("listen udp 5000")
receiver.send_event("listen tcp 5000")
receiver.send_event("listen tcp 5005")
receiver.send_event("logdata on")

# Create a second Mgen (will act as sender to above mgen udp receiver)
sender = mgen.Controller("sender")
sender.send_event("on 1 udp dst 127.0.0.1/5000 per [1 1024] count 1")

def send_example_payload_messages():
    # send a default UDP message
    sender.send_event(mgenEvent.TxEvent("UDP").send())

    # send a default TCP message
    sender.send_event(mgenEvent.TxEvent("TCP",2).send())

    # Send count UDP message via the subclass
    udp = mgen.mgenEvent.UDPEvent()
    udp.count = 3
    sender.send_event(udp.send())

    # Send a voip traffic for random interval
    sender.send_event(mgenEvent.VOIPEvent().send())
    # (Fixed flow id for voip for testing)
    sender.send_event("%d off 3" %(random.randrange(1,3)))

    # Send payload to trigger sending a tcp stream
    udp.count = 1
    udp.data = "02"
    udp.flow_id = 5
    udp.pattern = "periodic [1 512]"
    sender.send_event(udp.send())

def send_message_with_payload(cmd):
    # Send a simple text message
    print "Sending payload",cmd
    sender.send_event("on 2 udp dst 127.0.0.1/5000 per [1 1024] data [%s] count 1"
                  %cmd.encode('hex','strict').rstrip())


send_message_with_payload("Simple line of text")

# Monitor mgen receiver's output for events
for line in receiver:
    event = mgen.Event(line)
    print event.rx_time,event.size
    if event.data is not None:
        print "Payload>",event.data
    # handle payload or ack event
    #if (event.data != None):
    #    mgenEvent.handlePayload(sender,event)
    #else:
    #    mgenEvent.ackEvent(sender,event)

    print line
