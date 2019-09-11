#! /usr/bin/env python
# A basic mgen actor that can be instantiated as a zerorpc server for remote mgen control
# This relies on installing the mgen controller within the local python environment and a importable module
# See mgen install instructions and setup.py with mgen distribution
# Requires Zerorpc and optparse modules
# Joe Macker Jan 2015

import zerorpc
import mgen
import optparse, sys, os, datetime, signal

# Setup an actor response class with various methods
class MgenActor(object):
    
    def __init__(self, name):
        self.name = name
        self.sender = mgen.Controller(name)

    def send_sensor_payload(self, sensor_data, ip_addr, ip_port, interface,max_mgen=1400):
        ''' Sends binary based sensor payload
            
            RPC call to construct and send an mgen message with a binary 
            byte-based sensor payload as a python structure of some sort 
            (e.g., list of seq,sample)
            Required input:
            
            send_sensor_payload(sensor_data,ip_addr,ip_port,interface,max_mgen=1400)
            
            Because of mgen payload limits if length is longer the max_mgen it
            will truncate. Packet size will be autocalculated using the following
            ipv4_mgen_header = 54 + payload length (bytes)
        '''
        if len(sensor_data) > max_mgen:
            sensor_data = sensor_data[:max_mgen]
            size = max_mgen + 54
        else:
            size = len(sensor_data) + 54
        mgen_size = " per [1 {0}]" .format(unicode(str(size),'utf-8'))
        mgenevent = "on 1 udp dst " + str(ip_addr) + "/" + str(ip_port)\
                    + mgen_size\
                    + " count 1 interface " + str(interface)\
                    + " data [%s]" % sensor_data
                                      
        self.sender.send_event(mgenevent)
        print "Executed MGEN send !!"
        return mgenevent

    def send_text_payload(self, text, ip_addr, ip_port, interface, max_mgen=1400):
        ''' Sends text string as mgen payload
            
            RPC call to construct and send an mgen message with a text data payload
            Required input:
            
            send_text_payload(text,ip_addr,ip_port,interface)
            
            Because of mgen payload limits if length is longer the 255 it
            will truncate. Packet size will be autocalculated using the following
            ipv4_mgen_header = 54 + payload length (bytes)
        '''
        if len(text) > max_mgen:
            sensor_data = text[:max_mgen]
            size = max_mgen + 54
        else:
            size = len(text) + 54

        mgen_size = " per [1 {0}]" .format(unicode(str(size),'utf-8'))
        mgenevent = "on 1 udp dst " + str(ip_addr) + "/" + str(ip_port)\
                    + mgen_size\
                    + " count 1 interface " + str(interface)\
                    + " data [{0}]" .format(text.encode('hex','strict').rstrip())
                                        
        self.sender.send_event(mgenevent)
        print "Executed MGEN send !!"
        return mgenevent

    def send_event (self, mgen_event):
        ''' Send a raw mgen event
            
            Send Raw mgen event commands over the zerorpc connection
        '''
                                        
        self.sender.send_event(mgen_event)
        print "Executed MGEN send !!"
        return mgen_event

    def send_mgen_cmd(self, mgen_cmd):
        ''' Send a raw mgen event
            
            Send Raw mgen event commands over the zerorpc connection
        '''
                                        
        self.sender.send_command(mgen_cmd)
        print "Sent MGEN command !!"
        return mgen_cmd
    
    def my_name(self):
        ''' Returns name of controller instance'''
        return self.name

def shutdown(*args):
    print "Shutting down"
    sys.exit(0)

def main():
    usagestr = "usage: %prog [-h] [options] [args]"
    parser = optparse.OptionParser(usage = usagestr)

    parser.set_defaults(addr = "127.0.0.1")
    parser.set_defaults(port = 5555)
    parser.set_defaults(name = "mgenActor")
    parser.add_option("-a", "--addr", dest = "addr", type = str,
                      help = "addr to bind zerorpc server; default = %s" %
                      parser.defaults["addr"])
    parser.add_option("-p", "--port", dest = "port", type = int,
                      help = "port for zerorpc server; default = %s" %
                      parser.defaults["port"])
    parser.add_option("-n", "--name", dest = "name", type = str,
                      help = "name for mgen controller; default = %s" %
                      parser.defaults["name"])

    def usage(msg = None, err = 0):
        sys.stdout.write("\n")
        if msg:
            sys.stdout.write(msg + "\n\n")
        parser.print_help()
        sys.exit(err)

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    # parse command line options
    (options, args) = parser.parse_args()

    if options.port < 2000 or options.port > 100000:
        usage("invalid port: %s" % options.port)


    port=options.port
    server_url = "tcp://" + options.addr + ":%s" % str(port)
    print server_url
    s = zerorpc.Server(MgenActor(options.name))
    s.bind(server_url)
    s.run()

if __name__ == "__main__":
    main()
