import mgen

# Create an MgenController instance
# (an underlying "mgen" process is instantiated)
# Note the instance name passed in is completely _optional_
receiver = mgen.Controller("receiver")

# Send a global command
receiver.send_command("ipv4")

# Send a script event to enact immediately (mgen udp receiver)
receiver.send_event("listen udp 5000")

# Create a second Mgen (will act as sender to above mgen udp receiver)
sender = mgen.Controller("sender")

# "Manually"  start a flow from this sender
#sender.send_event("on 1 udp dst 127.0.0.1/5000 per [1 1024]")


# Here's another way to create a sender flow using the mgen.Flow class
flow2 = mgen.Flow(2, sender)
flow2.set_protocol('UDP')
flow2.set_destination('127.0.0.1', 5000)
flow2.set_pattern('periodic [0.1 64]')
flow2.set_count(6)
flow2.start()

# Here are some time-delay flow "mod" events for Flow 1 that adds a payload
# mgen.get_payload_from_string will simply validate that payload is < 255 at this point
payload = mgen.get_payload_from_string("Flow 1 initial payload")
if payload is not None:
    sender.send_event("3.0 mod 1 data [%s]" %payload)

# alternatively with no size validation
#sender.send_event("6.0 mod 1 data [%s]" % "Flow 1 modified payload".encode('hex','strict').rstrip())


# Monitor mgen receiver's output for events
updateCount = 1
for line in receiver:
    event = mgen.Event(line)
    # print the line received
    print line,
    # print payload content, if applicable
    if event.data:
        print "   (payload content is \"%s\")" % event.data
        
    # Here we dynamically update Flow 2's payload as we receive its messages
    if mgen.Event.RECV == event.type and 2 == event.flow_id:
        flow2.set_text_payload("hi, this flow 2 update %d" % updateCount)
        updateCount += 1
        if 3 == event.sequence:
            flow2.set_count(3)    
    
