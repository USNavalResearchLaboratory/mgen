
MGEN Agent for the Berkeley ns-2 Simulation Tool

This directory contains code for building "MGEN" as an "Agent" for the
Berkeley ns-2 simulation environment.

MGEN is based on the NRL "Protolib" which provide an abstraction of
timers and sockets for different operating systems.  Ns-2 is treated
as another "operating system" to some extent, and Protolib has an
"NsProtoAgent" base class which can be used to "port" applications
built using the Protolib pieces into ns-2 as agents which can operate
within the simulation environment.

This directory contains code for the "NsMgenAgent" C++ class which is
derived from the "NsProtoAgent" class.  The "NsMgenAgent" class serves
as a container/ controller for the "Mgen" class in the same way that
the "MgenApp" class acts as a container/controller for the real-world,
command-line "mgen" application.

Mgen commands may be passed to the ns-2 MGEN agent in ns-2 TCL scripts
as with other ns-2 agents.  The MGEN "start" command is overridden to
begin MGEN operation.  Here's is an abridged example of using the MGEN
agent in an ns-2 script:

#Create two MGEN  agents and attach to nodes
set mgen1 [new Agent/MGEN]
$ns_ attach-agent $n1 $mgen1

set mgen2 [new Agent/MGEN]
$ns_ attach-agent $n2 $mgen2

# Instead of using an MGEN script, we can use the MGEN 
# "event" command to insert MGEN script events.  Note 
# that at least one "event" MUST be inserted _before_
# the MGEN "start" is invoked.  (Note that MGEN scripts
# external to the ns-2 tcl script _can_ be use if
# desired, but it is easier to get the ns-2 node
# addresses from within the ns-2 script.  Ns-2 node 
# identifiers are use as network "addresses"
# in MGEN ns-2 operation. 
    
# 1) Have "mgen1" send a flow to mgen2 port 5002
# (Note the square brackets for the MGEN pattern must be escaped)

$ns_ at 0.0 "$mgen1 event {on 1 udp dst [$n2 node-addr]/5002 periodic \[10.0 1024\]}"

# 2) Have "mgen2" listen on port 5002 and log to file "mgenLog.drc"
$ns_ at 0.0 "$mgen2 event {listen udp 5002} output mgenLog.drc"

# 3) Start the MGEN agents
$ns_ at 0.0 "$mgen1 start"
$ns_ at 0.0 "$mgen2 start"

# 4) At time 50.0 seconds, modify the transmission pattern of "mgen1"
$ns_ at 50.0 "$mgen1 event {mod 1 poisson \[10.0 1024\]}"

The NRL "trpr" program can be used to analyze both any resulting ns-2 trace
files and/or resulting MGEN log files.  "mgen output /dev/null" can be
used to disable MGEN logging.

FILES:

nsMgenAgent.h     - Defines the C++ class "NsMgenAgent"

nsMgenAgent.cpp   - Implements the NsMgenAgent

nsNNN-Makefile.in - modified version of ns-N.NN
                    "Makefile.in" available in the
		    protolib src tree.
                    
(Note this Makefile.in also contains changes to build other NRL ns-2
"extensions" besides MGEN including the MDP and NORM reliable
multicast protocols.  These can be edited out or the use can download
the MDP and NORM source code too)

patch-ns.sh        - Simple shell script to provide
                     links to NRL protolib, mgen, mdp,
                     and norm code and the modified
                     Makefile.in given above.  Located
		     in protolib src tree.
                     
example.tcl        - ns-2 example script using MGEN

example.mgn        - MGEN script file for above example

BUILD:

After downloading and building the ns-2 distribution, you can
MGEN (and MDP and NORM) functionality by:

1) Modify "patch-ns.sh" to point to your ns-2 source code directory
   and Protolib and MGEN (and MDP and NORM if used) directories.
   
2) Change your working directory to the "patch-ns.sh" directory
   and execute "patch-ns.sh"
   
3) When prompted to replace your ns-2 "Makefile.in", allow it.

4) Change to your ns-2 source directory.

5) Type "./configure"

6) Type "make ns" to build ns-2 with the features.

You can test it with "ns example.tcl" to try out the MGEN.

TODO:

1)  The NsMgenAgent::GetPosition() function needs to be completed for simulations
with mobile nodes.

2)  Add a "nolog" command to MGEN to disable logging. (Meanwhile 
    "output /dev/null" can be used.

Brian Adamson
<adamson@itd.nrl.navy.mil>

8 May 2003



                     
  

