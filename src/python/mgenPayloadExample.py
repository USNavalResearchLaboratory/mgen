import mgen
import random
import struct
from ctypes import create_string_buffer
import time
import binascii

# Create an MgenController instance
# (an underlying "mgen" process is instantiated)
receiver = mgen.Controller()

# Send a global command
receiver.send_command("ipv4")

# Send a script event to enact immediately (mgen udp receiver)
receiver.send_event("listen udp 5000")
receiver.send_event("listen tcp 5000")
receiver.send_event("logdata on")

# Create a second Mgen (will act as sender to above mgen udp receiver)
sender = mgen.Controller("sender")

def send_message():
    sender.send_event(" on 1 udp dst 127.0.0.1/5000 per [1 1024]")

def send_simple_text_payload():
    # Send a simple text message
    sender.send_event("on 1 udp dst 127.0.0.1/5000 per [1 1024] data [%s] count 1"
                  %"Simple line of text".encode('hex','strict').rstrip())

def send_behavior_id():
    # Send "well known payload" that will trigger a "voip response"
    sender.send_event("on 2 udp dst 127.0.0.1/5000 per [1 1024] data [%s] count 1"
                  %"01".encode('hex','strict').rstrip())

def send_function_name():
    # Send a function  name - prepend with 03 function indicator for sample
    sender.send_event("on 3 udp dst 127.0.0.1/5000 per [1 1024] data [%s] count 1"
                  %"03stream_func".encode('hex','strict').rstrip())

def send_data_structure(eventType,eventValue,offsetTime,flowId):
    """ send a data structure in network order
    """
    # Create fixed pattern
    #pattern = "!H %ss %ss f" %(len(eventType),len(eventValue))
    pattern = "!H 3s 18s f"
    s = struct.Struct(pattern)

    # Create struct values
    values = (int(flowId), eventType, eventValue, float(offsetTime) )
    data_buffer = create_string_buffer(s.size)

    # pack struct
    s.pack_into(data_buffer,0,*values)
    buff = binascii.hexlify(data_buffer)

    # TBD: get rid of hexlify stuff?..
    sender.send_event("on 4 udp dst 127.0.0.1/5000 periodic [1 1024] data [ff%s] count 1"
                  %buff.encode('hex','strict').rstrip())

def handleFunction(payload):
    # See if it's a function
    try:
        methodToCall = globals()[payload]
        methodToCall(sender,event)
    except KeyError, e:
        # no action must be a string
        print "Not a function payload>", payload

def handleStruct(payload):
    # fudge for sample testing to get pattern length
    pattern = "!H 3s 18s f"

    newS = struct.Struct(pattern)
    unpacked_buffer = binascii.unhexlify(payload)
    data_buffer =  newS.unpack(str(unpacked_buffer))

    # execute command
    print "mgenEvent> %f %s %s %s" %(data_buffer[3], data_buffer[1], data_buffer[0], data_buffer[2])
    sender.send_event("%f %s %s %s" %(data_buffer[3],data_buffer[1], data_buffer[0], data_buffer[2]))

def handlePayload(sender, event):
    behavior_dict = {'01': voip_func,
                     '02': stream_func}

    # Decode hex data in event class?
    if event.data is None:
        return

    # Decoding hex data in event class
    payload = event.data #.decode('hex','strict')

    # if the payload is in our behavior dictionary, invoke the function
    if behavior_dict.has_key(str(payload[:2])):
        behavior_func = behavior_dict[payload[:2]]
        behavior_func(sender,event)
    else:
        # Is it a function call?
        if str(payload[:2]) == "03":
            handleFunction(payload[2:])
        else:
            # is it a struct?
            if str(payload[:1] == "ff"):
                print "payload>> ", payload
                handleStruct(payload[1:])
            else:
                print "Unhandled payload>", payload

def ackEvent(sender, event):
    sender.send_event("on 5 udp dst " + event.src_addr + "/5000 periodic [1 512] count 1")

def voip_func(sender, event):
    print "Sending voip event"
    sender.send_event("on 6 udp dst " + event.src_addr + "/5000 BURST [RANDOM 10.0 PERIODIC [10.0 2048] EXP 5.0] count 3")
    sender.send_event("%d off 4" % (random.randrange(1, 3)))

def stream_func(sender, event):
    print "Sending stream to %s" %event.src_addr
    sender.send_event("on 7 tcp dst " + event.src_addr + "/5000 periodic [1 387283] ")


eventTypes = ("RECV",  "RERR",  "SEND",  "JOIN",  "LEAVE",  "LISTEN", "IGNORE", "ON", "CONNECT",
    "ACCEPT", "SHUTDOWN", "DISCONNECT", "OFF", "START", "STOP")

# start sending a flow (1)
send_message()
#send_simple_text_payload()
#send_behavior_id()
#send_function_name()

# Send a data structure to mod the flow we started earlier
send_data_structure("mod","periodic [1 2048]",3.1345,1)
# Create a second struct to illustrate turning off a flow created earlier
# 2nd struct not parsing - perhaps our filter is off
#send_data_structure("off","",6.1342,1)

# Monitor mgen receiver's output for events
for line in receiver:
    event = mgen.Event(line)
    print line
    print event.rx_time,event.size
    # handle payload or ack recv events
    if eventTypes[event.type] == "RECV":
        if (event.data is not None):
            handlePayload(sender,event)
        else:
            ackEvent(sender,event)

