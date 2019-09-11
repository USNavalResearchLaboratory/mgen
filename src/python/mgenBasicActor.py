#! /usr/bin/env python

'''This module provides a basic mgen actor that can be instantiated as a zerorpc server for remote mgen control.

This relies on installing the mgen controller within the local python environment and a importable module.  See mgen install instructions and setup.py with mgen distribution. This module requires zerorpc and argparse modules'''

__author__      = "Joe Macker"
__copyright__   = ""

import zerorpc
import mgen
import argparse, sys, os, datetime, signal

global _HAVE_GPSPUB
try:
    __import__('imp').find_module('_gpsPub')
    import _gpsPub as gpsPub
    _HAVE_GPSPUB = True
    # Make things with supposed existing module
except ImportError:
    _HAVE_GPSPUB = False
    pass

# Setup an actor response class with various methods
class MgenActor(object):
    """This class is the basic Mgen Actor class object 

          :param name: required and provides the basic name of the actor
          :param hasGps: sets whether to use Gps shared memory approach
          :type name: string
          :type hasGps: boolean

    - and to provide sections such as **Example** using the double commas syntax::

          :Example:

          followed by a blank line !

      which appears as follow:

      :Example:

        s = zerorpc.Server(MgenActor(args.name))    
        s.bind(server_url)
        s.run()

    .. seealso:: 
        MGEN Reference and Users guide
        http://downloads.pf.itd.nrl.navy.mil/docs/mgen/mgen.html
        
    .. note:: 
        mgen.py module is required from an mgen install.

    """    
    def __init__(self, name, hasGps=True):
        self.name = name
        if _HAVE_GPSPUB is False:
            # Disable gps regardless of our init flag if
            # the gps module is not available 
            hasGps = False
        self.hasGps = hasGps
        if self.hasGps:
            self.gpsKey = os.getcwd() +  '/gpsKey'
            self.gpsHandle = gpsPub.GPSPublishInit(self.gpsKey)
            self.sender = mgen.Controller(name,self.gpsKey)
            self.gpsLat=None
            self.gpsLon=None
            self.gpsAlt=None
        else:
            self.sender = mgen.Controller(name)

        # TODO: add a python context manager so we can shutdown?
        # gpsPub.GPSPublishShutdown(self.gpsHandle,self.gpsKey)

    def send_sensor_payload(self, sensor_data, ip_addr, ip_port, interface):
        ''' Send a binary-based sensor payload using MGEN
            
            RPC call to construct and send an mgen message with a binary 
            byte-based sensor payload as a python structure of some sort 
            (e.g., list of seq,sample, dict)
            Required input:

        :param sensor_data: hexified sensor data for mgen packet data field
        :param ip_addr: ip destination address
        :param ip_port: ip destination port
        :param interface: network interface name to use as source
        :param max_mgen: maximum mgen packet size to use
        :type sensor_data: str
        :type ip_addr: str
        :type ip_port: int
        :type interface: str
        :type max_mgen: int
        :returns: MGEN event line that was invoked by agent
        :rtype: str 
        
        ..warning::
            Because of mgen payload limits if length is longer the max_mgen it
            will truncate. Packet size will be autocalculated using the following
            ipv4_mgen_header = 54 + payload length (bytes)
        '''
        max_mgen=1400
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

    def send_text_payload(self, text, ip_addr, ip_port, interface, flowid):
        ''' Send a text-based payload using MGEN
            
            RPC call to construct and send an mgen message with a text payload
            Required input:
            
        :param text: text for mgen packet data field
        :param ip_addr: ip destination address
        :param ip_port: ip destination port
        :param interface: network interface name to use as source
        :param max_mgen: maximum mgen packet size to use
        :type sensor_data: str
        :type ip_addr: str
        :type ip_port: int
        :type interface: str
        :type flowid: int
        :returns: MGEN event line that was invoked by agent
        :rtype: str 
        
        ..warning::
            Because of mgen payload limits if length is longer the max_mgen it
            will truncate. Packet size will be autocalculated using the following
            ipv4_mgen_header = 54 + payload length (bytes)
        '''
        max_mgen=1400
        if len(text) > max_mgen:
            sensor_data = text[:max_mgen]
            size = max_mgen + 54
        else:
            size = len(text) + 54

        mgen_size = " per [1 {0}]" .format(unicode(str(size),'utf-8'))
        mgenevent = "on " + str(flowid) + " udp dst " + str(ip_addr) + "/" + str(ip_port)\
                    + mgen_size\
                    + " count 1 interface " + str(interface)\
                    + " data [{0}]" .format(text.encode('hex','strict').rstrip())
                                        
        self.sender.send_event(mgenevent)
        print "Executed MGEN send !!"
        return mgenevent

    def send_event (self, mgen_event):
        ''' Send a raw mgen event
            
            Function to send a raw mgen event conformant with 
            MGEN Reference and Users Guide.
            
        :param mgen_event: MGEN event
        :returns: MGEN event line that was invoked by agent
        :rtype: str 
        
        ..note:
            Refer to MGEN Reference and Users Guide.
        '''
                            
        self.sender.send_event(mgen_event)
        print "Executed MGEN send !!"
        return mgen_event

    def send_mgen_cmd(self, mgen_cmd):
        ''' Send an mgen command
            
            Function to send an mgen command conformant with 
            MGEN Reference and Users Guide.
            
        :param mgen_cmd: MGEN command
        :returns: MGEN command line that was invoked by agent
        :rtype: str 
        
        ..note:
            Refer to MGEN Reference and Users Guide.
        '''
                                        
        self.sender.send_command(mgen_cmd)
        print "Sent MGEN command !!"
        return mgen_cmd
    
    def my_name(self):
        ''' Function to return name of actor to remote RPC client
            
        :returns: name of Actor Class instance
        :rtype: str 
        
        '''
        return self.name

    def has_gps(self):
        ''' Function to return whether has_gps is enabled
        
            has_gps is a state variable that determines whether
            gps reading and writing is supported for an mgen Actor instance
            
        :returns: has_gps
        :rtype: boolean 
        
        '''
        return self.hasGps

    def publish_gps_pos(self,lat,lon,alt):
        ''' Helper function to write lat,lon,alt to shared memory location
            
        :returns: N/A
        
        '''
        if self.hasGps:
            try:
                self.gpsLat = float(lat)
                self.gpsLon = float(lon)
                self.gpsAlt = float(alt)
            except ValueError:
                print "publish_gps_pos() error converting values"
                return
            gpsPub.GPSPublishPos(self.gpsHandle,self.gpsLat, self.gpsLon, self.gpsAlt)
        else:
            print "publish_gps_pos() gps not enabled."
        return

    def get_gps_pos(self):
        ''' Helper function to return lat,lon,alt from shared memory location
            
        :returns: gpsLat,gpsLon,gpsAlt
        
        '''
        if not self.hasGps:
            print "get_gps_pos() gps not enabled."
            return False
        else:
            return self.gpsLat,self.gpsLon,self.gpsAlt

def shutdown(*args):
    print "Shutting down"
    sys.exit(0)

def main():

    parser = argparse.ArgumentParser()
    parser.add_argument("-a", "--addr", 
                        type = str, 
                        default = "127.0.0.1",
                        help = "addr to bind zerorpc server")
    parser.add_argument("-p", "--port", 
                        dest = "port", 
                        type = int, 
                        default=10101,
                      help = "port for zerorpc server")
    parser.add_argument("-n", "--name", 
                        dest = "name", 
                        type = str,
                        default = "mgenActor",
                      help = "name for mgen controller")
    parser.add_argument("-d", "--disableGps", dest = "hasGps", action='store_false',
                      help = "disable gps for this actor; gps is enabled by default")
    def usage(msg = None, err = 0):
        sys.stdout.write("\n")
        if msg:
            sys.stdout.write(msg + "\n\n")
        parser.print_help()
        sys.exit(err)

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    # parse command line options
    args = parser.parse_args()

    if args.port < 2000 or args.port > 100000:
        usage("invalid port: %s" % args.port)


    port=args.port
    server_url = "tcp://" + args.addr + ":%s" % str(port)
    print server_url
    hasGps = args.hasGps
    if _HAVE_GPSPUB is False:
        hasGps = False
        print "_gpsPub not installed on this system, disabling gps support"
    s = zerorpc.Server(MgenActor(args.name,hasGps))    
    s.bind(server_url)
    s.run()

if __name__ == "__main__":
    main()
