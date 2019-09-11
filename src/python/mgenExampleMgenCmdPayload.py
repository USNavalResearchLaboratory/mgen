import mgen
import time

# Create an MgenController instance
# (an underlying "mgen" process is instantiated)
receiver = mgen.Controller()

# Send a global command
receiver.send_command("ipv4")

# Send a script event to enact immediately (mgen udp receiver)
receiver.send_event("listen udp 5000")

receiver.send_event("txlog")

# Create a second Mgen (will act as sender to above mgen udp receiver)
sender = mgen.Controller("sender")


# Set first byte to high values to indicate mgen payload
# direct receiver to listen to port 5010
payload = mgen.GetPayloadFromString("FFLISTEN UDP 5010")
if payload is not None:
    sender.send_event("ON 11 udp dst 127.0.0.1/5000 periodic [1 2048] data [%s] count 1" %payload)

# Set first byte to high values to indicate mgen payload
# direct receiver to start an udp flow
payload = mgen.GetPayloadFromString("FFON 21 udp dst 127.0.0.1/5010 periodic [1 512]")
if payload is not None:
    sender.send_event("ON 12 udp dst 127.0.0.1/5000 periodic [1 4096] data [%s] count 1" %payload)

# Set first byte to high values to indicate mgen payload
# direct receiver to stop udp flow
payload = mgen.GetPayloadFromString("FF10.0 OFF 21")
if payload is not None:
    sender.send_event("ON 13 udp dst 127.0.0.1/5000 periodic [1 4096] data [%s] count 1" %payload)


# Monitor mgen receiver's output for events
for line in receiver:
    event = mgen.Event(line)
    # print the line received
    print line
    # print unformatted payload
    if event.data is not None:

        if event.GetPayloadMgenCmd():
            print "mgen payload command>%s" % event.GetPayloadMgenCmd()
            receiver.send_event(event.data[2:])
        else:
            print event.data

