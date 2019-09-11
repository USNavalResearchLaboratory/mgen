import os
import subprocess
import protokit
import datetime
import mgenEvent

class gps:
    def __init__(self, lat=None, lon=None, alt=None):
        if lat is None or lon is None:
            self.valid = False
        else:
            self.valid = True
            self.lat = lat
            self.lon = lon
            self.alt = alt


def TimeFromString(text):
    """Convert MGEN timestamp string to Python datetime.time object"""
    field = text.split(':')
    hr = int(field[0])
    mn = int(field[1])
    field = field[2].split('.')
    sec = int(field[0])
    usec = int(field[1])
    return datetime.time(hr, mn, sec, usec)


class Controller:
    def __init__(self, instance=None):
        """Instantiate an mgen.Controller
        The 'instance' name is optional.  If it is _not_ given, an mgen instance named
        "mgen-<processID>" where the <processID> is the process id of the Python 
        script.  If an mgen instance with the given name already exists, any commands
        this controller issues will be to that mgen instance.  Otherwise a new mgen
        instance is created.  Note that the mgen output can be monitored only for
        the mgen instance the controller creates.
        :type self: object
        """
        args = ['mgen', 'flush', 'instance']
        if instance is None:
            self.instance_name = 'mgen-' + str(os.getpid())
        else:
            self.instance_name = instance
        args.append(self.instance_name)
        fp = open(os.devnull, 'w')  # redirect stderr to /dev/null to hide
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
        self.mgen_pipe = protokit.Pipe("MESSAGE")
        self.mgen_pipe.Connect(self.instance_name)

    def __del__(self):
        self.mgen_pipe.Close()
        self.proc.terminate()
        self.proc.wait()

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


class MyEnum(tuple): __getattr__ = tuple.index
Protocol = MyEnum(["UDP", "TCP", "SINK"])

class Event:
    def __init__(self, text=None, flow_id=None):
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
            self.InitFromString(text)

    # Simple "enum" of valid MGEN event types     
    RECV, RERR, SEND, JOIN, LEAVE, LISTEN, IGNORE, ON, CONNECT, \
    ACCEPT, SHUTDOWN, DISCONNECT, OFF, START, STOP = range(15)

    def GetEventType(self, text):
        etype = Event.__dict__[text];  #get(text)
        if etype is None:
            return None
        else:
            return etype

    def InitFromString(self, text):
        """ Initialize mgen.Event from mgen log line """
        # MGEN log format is "<time> <eventType> <attr1>><value> <attr2>><value> ...
        field = text.split()
        self.rx_time = TimeFromString(field[0])
        print "\"%s\"" % field[1]
        self.type = self.GetEventType(field[1])
        for item in field[2:]:
            key, value = item.split('>')
            if ('proto' == key):
                self.protocol = value
            elif ('flow' == key):
                self.flow_id = int(value)
            elif ('seq' == key):
                self.sequence = int(value)
            elif ('src' == key):
                addr, port = value.split('/')
                self.src_addr = addr
                self.src_port = port
            elif ('dst' == key):
                addr, port = value.split('/')
                self.dst_addr = addr
                self.dst_port = port
            elif ('sent' == key):
                self.tx_time = TimeFromString(value)
            elif ('size' == key):
                self.size = int(value)
            elif ('gps' == key):
                field = value.split(',')
                if ("INVALID" != field[0]):
                    self.gps = gps(field[1], field[2], field[3])
            elif ('data' == key):
                length, data = value.split(':')
                self.data_length = length
                self.data = data.decode('hex','strict')
                data_buffer = None;  # TBD convert MGEN data string to binary buffer


