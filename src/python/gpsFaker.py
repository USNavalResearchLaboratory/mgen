import _gpsPub as gpsPub
import argparse
import ctypes
import datetime
import random
import math
import threading
import time

class timeval():
    def __init__(self,tv_sec=0.0,tv_usec=0.0):
        self.tv_sec = tv_sec
        self.tv_usec = tv_usec
    _fields_ = [("tv_sec", ctypes.c_long),("tv_usec", ctypes.c_long)]

class GPSPosition():
    def __init__(self,x,y,z,gps_time_sec=0,gps_time=0,sys_time=0,xyvalid=True,zvalid=True,tvalid=True,stale=False):
        self.x = x
        self.y = y
        self.z = z
        self.gps_time = gps_time
        self.sys_time = sys_time
        self.xyvalid = xyvalid
        self.zvalid = zvalid
        self.tvalid = tvalid
        self.stale = stale

    _fields_ = [("x",ctypes.c_double),("y",ctypes.c_double),("z",ctypes.c_double),("gpsTime",timeval),("sysTime",timeval),("xyvalid",ctypes.c_int),("zvalid",ctypes.c_int),("tvalid",ctypes.c_int),("stale",ctypes.c_int)]


def publish_random_updates(handle,lon,lat,alt,interval,radius):
    # Convert radius from meters to degrees
    radiusInDegrees = radius / 111000.0

    u = random.uniform(1,100)
    v = random.uniform(1,100)
    w = radiusInDegrees * math.sqrt(u)
    t = 2 * math.pi * v
    x = w * math.cos(t)
    y = w * math.sin(t)

    # Adjust the x-coor for the shrinking of the east-west distances
    # aren't we fancy
    new_x = x / math.cos(lat)

    new_lon = new_x + lon
    new_lat = y + lat

    print "lat %f lon %f " %(new_lat,new_lon)

    gpsPub.GPSPublishPos(handle,new_lat,new_lon,alt)

def main():
    usagestr = "usage: %prog [-h] [options] [args]"
    parser = argparse.ArgumentParser()

    parser.add_argument("-f",default="/tmp/gpskey",dest="gpsFile",type=str,help="Location of gps shared memory file")
    parser.add_argument("--lat",default="38.828533",dest="lat",type=str,help="lat")
    parser.add_argument("--lon",default="-77.028633",dest="lon",type=str,help="lon")
    parser.add_argument("--alt",default="0.0",dest="alt",type=str,help="alt")
    parser.add_argument("-r","--random",dest="random",action='store_true',help="Publish random gps reports. Enter lat,lon,alt to offset random reports from given location.")
    parser.add_argument("-i","--interval",default="1",dest="interval",type=float,help="Publish interval")
    parser.add_argument("--radius",default="200",dest="radius",type=int,help="Radius of gps reports from origin")
    args = parser.parse_args()

    print args.gpsFile
    lat = float(args.lat)
    lon = float(args.lon)
    alt = float(args.alt)

    handle = gpsPub.GPSPublishInit(args.gpsFile)

    if args.random is True:
        while True:
            publish_random_updates(handle,lon,lat,alt,args.interval,args.radius)
            time.sleep(args.interval)
    else:
        gpsPub.GPSPublishPos(handle,lat,lon,alt)

    gpsPub.GPSPublishShutdown(handle,args.gpsFile)

if __name__ == "__main__":
    main()
    

