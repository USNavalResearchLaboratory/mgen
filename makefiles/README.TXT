                MGEN 4.x for UNIX

This directory contains Makefiles for Unix platforms and any
Unix-specific source code files.  Build instructions are given
below.  To run "mgen", see the on-line or included HTML
documentation.

The MGEN web site is <http://mgen.pf.itd.nrl.navy.mil>

BUILD INSTRUCTIONS:

The appropriate Makefile for your platform can be determined by
the extension to the Makefile name.  For example, the Linux
Makefile is "Makefile.linux".  I just haven't had time to learn
configure.  This would be a great contribution to MGEN if
someone would like to tackle it.

To build:

1) Find the appropriate "Makefile.<os>" according to your
operating system type (<os>).

2) Type: make -f Makefile.<os> mgen

   (Example for Linux: "make -f Makefile.linux mgen")

3) "mgen" is the resulting executable program.

NOTES:

1)  The "Makefile.<os>" contain build macro definitions and
    paths which tend to be platform specific.  One of these should
    be pretty easily adapted to support other platforms not
    listed. 

2)  The "Makefile.common" file contains the primary make
    information including source files and some common options.

3)  Performance can be further optimized by undefining the
    "PROTO_DEBUG" macro but at a loss of debug messages.

4)  For FreeBSD (or other platforms which might no have
    the Gnu "make" program).  You may need to first go
    to the "protolib/unix" directory and type
    "make -f Makefile.<os> libProto.a" _before_ doing
    the MGEN build.  This is because the "Makefile.common"
    uses the Gnu make "-C" option to build the Protolib
    code upon which mgen depends.

5)  I have not tested the Makefiles on all of the platforms so
    some tweaking may be required.  These Makefiles are still a work 
    progress.

6)  Feel free to email us with questions.

-------------

Brian Adamson adamson@itd.nrl.navy.mil

Hal Greenwald hgreenwald@itd.nrl.navy.mil

5 September 2002
