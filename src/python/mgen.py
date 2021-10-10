import os
import subprocess
import protokit
import sys
import shlex
import datetime
import struct

import warnings
import functools
import tempfile

# Set to True to enable DeprecationWarning messages
ENABLE_DEPRECATION_WARNINGS = True

def deprecated(func):
    '''This is a decorator which can be used to mark functions
    as deprecated. It will result in a warning being emitted
    when the function is used.'''
    @functools.wraps(func)
    def new_func(*args, **kwargs):
        if ENABLE_DEPRECATION_WARNINGS:
            warnings.simplefilter('always', DeprecationWarning)#turn off filter 
            warnings.warn_explicit(
                "Call to deprecated function {}.".format(func.__name__),
                category=DeprecationWarning,
                filename=func.func_code.co_filename,
                lineno=func.func_code.co_firstlineno + 1
            )
            warnings.simplefilter('default', DeprecationWarning) #reset filter
        return func(*args, **kwargs)
    return new_func


class gps:
    def __init__(self, lon=None, lat=None, alt=None):
        if lat is None or lon is None:
            self.valid = False
        else:
            self.valid = True
            self.lat = lat
            self.lon = lon
            self.alt = alt

def get_payload_from_string(text):
    if len(text) > 255:
        sys.stderr.write("get_payload_from_string() Error: Payload > 255: %s\n" % text)
        return None
    return text.encode('hex','strict').rstrip()
    
@deprecated
def GetPayloadFromString(text):
    return get_payload_from_string(text)

def get_time_from_string(text):
    """Convert MGEN timestamp string to Python datetime.time object"""
    field = text.split(':')
    hr = int(field[0])
    mn = int(field[1])
    field = field[2].split('.')
    sec = int(field[0])
    usec = int(field[1])
    return datetime.time(hr, mn, sec, usec)   
    
@deprecated
def TimeFromString(text):
    return get_time_from_string(text)
    

def protocol_is_valid(protocol):
    protocol_dict = dict.fromkeys(["udp", "tcp", "sink"], True)    
    if protocol_dict.get(protocol.lower()):
        return True
    else:
        return False    

# TBD - create a mgen.Flow class" that contains MGEN flow description/parameters
# (In turn a "Flow" member could be used in the mgen.Event below for events
# that pertain to a flow such as SEND, RECV, etc)

class Flow:
    """ The mgen.Flow class contains parameters for a Flow
    """
    def __init__(self, flowId, controller = None):
        self.flow_id = flowId  ;# must be unique per controller/mgen instance
        self.protocol = None
        self.dst_addr = None
        self.dst_port = None
        self.pattern = None
        self.src_port = None
        self.interface = None
        self.count = None
        self.sequence = None
        self.tos = None
        self.ttl = None
        self.data = None
        self.data_length = 0
        self.controller = None
        if controller is not None:
            controller.add_flow(self)
        self.active = False
        
    def is_valid(self):
        """Checks that the minimum parameters for a valid flow are set"""
        if self.flow_id is None:
            return False
        elif self.protocol is None:
            return False
        elif self.dst_addr is None:
            return False
        elif self.dst_port is None:
            return False
        elif self.pattern is None:
            return False
        else:
            return True
            
    def start(self, delay=0.0):
        if self.controller and self.is_valid():
            cmd = ""
            if (delay > 0.0):
                cmd += str(delay) + " "
            cmd += "on " + str(self.flow_id) 
            cmd += " " + self.protocol + " dst " + str(self.dst_addr) + '/' + str(self.dst_port)
            cmd += " " + str(self.pattern)
            if self.src_port is not None:
                cmd += " src " + str(self.src_port)
            if self.interface is not None:
                cmd += " interface " + self.interface
            if self.count is not None:
                cmd += " count " + str(self.count)
            if self.sequence is not None:
                cmd += " sequence " + str(self.sequence)
            if self.tos is not None:
                cmd += " tos " + str(self.tos)
            if self.ttl is not None:
                cmd += " ttl " + str(self.ttl)
            if self.data is not None:
                cmd += " data [" + self.data + "]"
            self.controller.send_event(cmd)
            self.active = True
                
    def stop(self):
        if self.controller and self.active:
            cmd = "off " + str(self.flow_id)
            self.controller.send_event(cmd)
            self.active = False   
            
    def set_flow_id(self, flowId):
        if (self.flow_id != flowId):
            restart = self.active
            if restart:
                self.stop()
            # If this flow is attached to a controller,
            # we need remove/add since they are indexed 
            # by flowId
            if self.controller:
                controller.remove_flow(self)
            self.flow_id = flowId
            if self.controller:
                controller.add_flow(self)
            if restart:
                self.start()
                
    def set_protocol(self, protocol):
        if not protocol_is_valid(protocol):
            return False
        self.protocol = protocol
        if self.active:
            self.stop()
            self.start()
            
    def set_destination(self, addr, port):
        if addr is None:
            raise ValueError('A valid IP address must be supplied!')
        self.dst_addr = addr
        self.dst_port = port
        if self.active:
            cmd = "mod %d dst %s/%d" % (self.flow_id, addr, port)
            self.controller.send_event(cmd)
            
    def set_interface(self, iface):
        self.interface = iface
        if self.active:
            cmd = "mod %d interface %s" % (self.flow_id, interface)
            self.controller.send_event(cmd)
            
    def set_source(self, srcPort):
        self.src_port = srcPort
        if self.active:
            cmd = "mod %d src %d" % (self.flow_id, srcPort)
            self.controller.send_event(cmd)
            
    def set_pattern(self, pattern):
        self.pattern = pattern
        if self.active:
            cmd = "mod %d %s" % (self.flow_id, pattern)
            self.controller.send_event(cmd)
            
    def set_count(self, count):
        self.count = int(count)
        if self.active:
            cmd = "mod %d count %d" % (self.flow_id, self.count)
            self.controller.send_event(cmd)
            
    def set_sequence(self, sequence):
        self.sequence = int(sequence)
        if self.active:
            cmd = "mod %d sequence %d" % (self.flow_id, self.sequence)
            self.controller.send_event(cmd)
            
    def set_tos(self, tos):
        self.tos = tos
        if self.active:
            cmd = "mod %d tos %s" % (self.flow_id, self.tos)
            self.controller.send_event(cmd)
            
    def set_ttl(self, ttl):
        self.ttl = ttl
        if self.active:
            cmd = "mod %d ttl %s" % (self.flow_id, self.ttl)
            self.controller.send_event(cmd)
            
    # TBD - we could store the flow payload data more efficiently 
    #       as the raw data instead of hex-encoded 
    def set_text_payload(self, text):
        self.data = text.encode('hex','strict').rstrip()
        if self.active:
            cmd = "mod %d data [%s]" % (self.flow_id, self.data)
            self.controller.send_event(cmd)
            
    def get_text_payload(self):
        if self.data:
            return self.data.decode('hex','strict')
        else:
            return None
            
    def set_binary_payload(self, buf):
        self.data = binasci.hexlify(buf)
        if self.active:
            cmd = "mod %d data [%s]" % (self.flow_id, self.data)
            self.controller.send_event(cmd)
            
    def set_struct_payload(self, theStruct, *args):
        """ Uses Python struct module"""
        self.data = theStruct.pack(*args).encode('hex','strict').rstrip()
        if self.active:
            cmd = "mod %d data [%s]" % (self.flow_id, self.data)
            self.controller.send_event(cmd)

class Event:
    """ The mgen.Event class provides information on logged events
        (or potentially also events that may invoked (i.e. actions)
    """
    def __init__(self, text = None):
        self.rx_time = datetime.time()
        self.type = None
        self.protocol = None
        self.flow_id = None
        self.sequence = None
        self.src_addr = None
        self.src_port = None
        self.dst_addr = None
        self.dst_port = None
        self.tx_time = datetime.time()
        self.size = 0
        self.gps = None
        self.data = None
        self.data_length = None
        if text is not None:
            self.init_from_string(text)
    
    # A sort of "enum" of valid MGEN event types     
    RECV,  RERR,  SEND,  JOIN,  LEAVE,  LISTEN, IGNORE, ON, CONNECT, \
    ACCEPT, SHUTDOWN, DISCONNECT, OFF, START, STOP = range(15)
    
    def get_event_type(self, text):
        etype = Event.__dict__[text.upper()]
        if etype is None:
            return None
        else:
            return etype
            
    @deprecated
    def GetEventType(self, text):
        return self.get_event_type(text)
            
    def has_payload(self):
        return self.data is not None
                
    def get_payload(self):
        return self.data
    
    @deprecated
    def GetPayload(self):
        return self.get_payload()
        
    def get_struct_payload(self, theStruct):
        '''This returns a tuple according to theStruct format'''
        return theStruct.unpack(self.data)
        
    @deprecated
    def GetStructPayload(self, theStruct):
        return self.get_struct_payload(theStruct)

    # These "MgenCmd" payload methods will likely be deprecated?
    @deprecated
    def IsPayloadMgenCmd(self):
        return self.data[0:2] == 'FF'
    
    @deprecated
    def GetPayloadMgenCmd(self):
        if self.data[0:2] == 'FF':
            return self.data[2:]
        else:
            return None
              
    def init_from_string(self, text):
        ''' Initialize mgen.Event from mgen log line '''
        # MGEN log format is "<time> <eventType> <attr1>><value> <attr2>><value> ...
        field = text.split()
        self.rx_time = get_time_from_string(field[0])
        self.type = self.get_event_type(field[1])
        for item in field[2:]:
            key,value = item.split('>')
            if ('proto' == key):
                self.protocol = value
            elif ('flow' == key):
                self.flow_id = int(value)
            elif ('seq' == key):
                self.sequence = int(value)
            elif ('src' == key):
                addr,port = value.split('/')
                self.src_addr = addr
                self.src_port = port
            elif ('dst' == key):
                addr,port = value.split('/')
                self.dst_addr = addr
                self.dst_port = port
            elif ('sent' == key):
                self.tx_time = get_time_from_string(value)
            elif ('size' == key):
                self.size = int(value)
            elif ('gps' == key):
                field = value.split(',')
                if ("INVALID" != field[0]):
                    self.gps = gps(field[1], field[2], field[3])
            elif ('data' == key):
                length,data = value.split(':')
                self.data = data.decode('hex','strict')
                self.data_length = length
      
    @deprecated          
    def InitFromString(self, text):
        return self.init_from_string(text)

class Controller:
    def __init__(self, instance=None, gpsKey=None):
        """Instantiate an mgen.Controller
        The 'instance' name is optional.  If it is _not_ given, an mgen instance named
        "mgen-<processID>-<objectID>" where the <processID> is the process id of the Python 
        script in hexadecimal notation and the <objectId> is the Python object id() of this
        Controller.  If an mgen instance with the given name already exists, any commands
        this Controller issues will be to that mgen instance.  Otherwise a new mgen
        instance is created.  Note that the mgen output can be monitored only for
        the mgen instance the controller creates, if applicable.
        """
        self.flow_dict = {}
        self.sink_proc = None
        self.gpsKey = gpsKey
        args = ['mgen', 'flush']
        if instance is None:
            pidHex = "%x" % os.getpid() 
            idHex = "%x" % id(self) 
            self.instance_name = 'mgen-' + pidHex + '-' + idHex 
        else:
            self.instance_name = instance
        
        if self.gpsKey is not None:
            args.append('gpskey')
            args.append(self.gpsKey)
            
        args.append('instance')
        args.append(self.instance_name)

        # TBD - should we try to connect to the instance_name to
        # see if it already exists instead of always creating a 
        # child process???
        self.mgen_pipe = protokit.Pipe("MESSAGE")
        try:
            self.mgen_pipe.Connect(self.instance_name)
            self.proc = None
        except:
            fp = open(os.devnull, 'w')  ;# redirect stderr to /dev/null to hide
            self.proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=fp)
            fp.close()
            # Read the mgen stdout and find the "START event to confirm it 
            # has launched successfully before trying to connect to it
            # (TBD - parse and save START event info for first readline() output)
            for line in self:
                field = line.split()
                if (len(field) > 1) and (field[1] == "START"):
                    break
            # Connect a ProtoPipe to the mgen instance to be able to control
            # the mgen instance as needed in the future
            self.mgen_pipe.Connect(self.instance_name)
            
    def __del__(self):
        self.shutdown()
                
    def shutdown(self):
        self.mgen_pipe.Close()
        if self.proc is not None:
            self.proc.terminate()
            self.proc.wait()
            self.proc = None
        if self.sink_proc is not None:
            self.sink_proc.terminate()
            self.sink_proc.wait()
            (txPipePath,rxPipePath) = self.get_sink_paths()
            try:
                os.unlink(txPipePath)
                os.unlink(rxPipePath)
            except:
                pass
        
     # These next two methods let us get output lines from the mgen 
     # instance in a Python for-in loop (e.g., "for line in mgen:")
     # (Note the instance needs to be one we created)
    def __iter__(self):
        return self
        
    def next(self):
        line = self.readline()
        if line:
            return line
        else:
            raise StopIteration
            
    # Read one line of mgen output
    def readline(self):
        """ Read one line of mgen output.  Note that the mgen output can be 
        monitored only for the mgen instance the controller creates.
        """
        return self.proc.stdout.readline()
        
    def send_command(self, cmd):
        """ Send a command to the associated mgen process.  The command is any 
        valid command-line per the MGEN User's Guide
        """
        self.mgen_pipe.Send(cmd)
        
    def send_event(self, event):
        """ Send an event to the associated mgen process.  The event is any 
        valid MGEN script line. E.g., "listen udp 5000".  The event will be
        process immediately unless a time is given.
        """
        cmd = "event " + event
        self.mgen_pipe.Send(cmd)
        
    def get_sink_paths(self):
        txPipeName = self.instance_name + "-txPipe"
        rxPipeName = self.instance_name + "-rxPipe"
        txPipePath = os.path.join(tempfile.gettempdir(), txPipeName)
        rxPipePath = os.path.join(tempfile.gettempdir(), rxPipeName)
        return (txPipePath, rxPipePath)
        
    def set_sink(self, sinkCmd):
        (txPipePath,rxPipePath) = self.get_sink_paths()
        if self.sink_proc is not None:
            self.sink_proc.terminate()
            self.sink_proc.wait()
            self.sink_proc = None
        try:
            os.unlink(txPipePath)
            os.unlink(rxPipePath)
        except:
            pass
        if sinkCmd is None:
            return
        os.mkfifo(txPipePath)
        os.mkfifo(rxPipePath)
        self.send_command('sink %s' % txPipePath)
        self.send_command('source %s' % rxPipePath)
        # This implements "cat txPipePath | sinkCmd > rxPipePath" to "wire up" our sinkCmd
        # (We use 'cat' since "tx_pipe" isn't ready until mgen sinks a flow to it.
        #  This lets the 'sinkCmd' do it's thing even if mgen is recv-only.)
        try:
            p1 = subprocess.Popen(['cat', txPipePath], stdout=subprocess.PIPE)
        except:
            os.unlink(txPipePath)
            os.unlink(rxPipePath)
            raise
        # Note this will block until the above "source rxPipePath" command is completed
        rp = open(rxPipePath, 'w')
        try:
            np = open(os.devnull, 'w')  ;# redirect stderr to /dev/null to hide
            self.sink_proc = subprocess.Popen(shlex.split(sinkCmd), stdin=p1.stdout, stdout=rp, stderr=np)
            np.close()
        except:
            # clean up stuff upon failure
            rp.close()
            p1.terminate()
            p1.wait()
            os.unlink(txPipePath)
            os.unlink(rxPipePath)
            raise
        # Close these up as they now belong to self.sink_proc
        rp.close()
        p1.stdout.close()
        
    def add_flow(self, flow):
        oldFlow = self.flow_dict.get(flow.flow_id)
        if oldFlow is not None:
            self.remove_flow(oldFlow)
        self.flow_dict[flow.flow_id] = flow
        flow.controller = self
        
    def get_flow(self, flowId):
        return flow_dict.get(flowId)
        
    def remove_flow(self, flow):
        if flow.active:
            flow.stop()
        del self.flow_dict[flow.flow_id]
        flow.controller = None

