# Mgen agent example

# Create multicast enabled simulator instance
set ns_ [new Simulator -multicast on]
$ns_ multicast

# Turn on "ns" and "nam" trace files
set f [open "example.tr" w]
$ns_ trace-all $f
set nf [open "example.nam" w]
$ns_ namtrace-all $nf

# Create two nodes
set n1 [$ns_ node]
set n2 [$ns_ node]

# Put a link between them
$ns_ duplex-link $n1 $n2 1Mb 100ms DropTail
$ns_ queue-limit $n1 $n2 100
$ns_ duplex-link-op $n1 $n2 queuePos 0.5
$ns_ duplex-link-op $n1 $n2 orient right

# Configure multicast routing for topology
#set mproto DM
#set mrthandle [$ns mrtproto $mproto {}]

set mproto DM
set mrthandle [$ns_ mrtproto $mproto {}]
# if {$mrthandle != ""} {
#     $mrthandle set_c_rp [list $n1]
#}

# 5) Allocate a multicast address to use
set group [Node allocaddr]
   
puts "Creating MGEN agents ..."   
# Create two MGEN  agents and attach to nodes
set mgen1 [new Agent/MGEN]
$ns_ attach-agent $n1 $mgen1
$mgen1 debug 12

set mgen2 [new Agent/MGEN]
$ns_ attach-agent $n2 $mgen2

# First an example of MGEN/UDP
# Instead of using an MGEN script, we use the MGEN "event" command
# to dynamically insert MGEN script events.
# Note that at least one "event" MUST be inserted _before_
# the MGEN "start" is invoked.
    
# Startup "mgen1", sending a flow to mgen2 port 5002 (no logging for this agent)
# (Note the square brackets for the MGEN pattern must be escaped"
#$ns_ at 0.0 "$mgen1 startup event {on 1 udp dst [$n2 node-addr]/5002 periodic \[10.0 1024\]} nolog"
$ns_ at 0.0 "$mgen1 startup event {on 1 udp dst $group/5002 periodic \[10.0 1024\]} nolog"

# Startup "mgen2", listening on port 5002 and log to "mgenLog.drc"
$ns_ at 0.0 "$mgen2 startup event {listen udp 5002} event {join $group} output mgenLog.drc"
#$ns_ at 0.0 "$mgen2 startup event {listen udp 5002} output mgenLog.drc"

# Now, an example of MGEN/TCP
# TCP server ...
#$ns_ at 0.0 "$mgen2 startup event {listen tcp 5002} output mgenLog.drc"
# TCP client (sending data to server)
#$ns_ at 0.1 "$mgen1 startup event {on 1 tcp dst [$n2 node-addr]/5002 periodic \[10.0 1024\]} nolog"

puts "Starting simulation ..." 


# At time 50.0 seconds, modify the transmission pattern of "mgen1"
#$ns_ at 50.0 "$mgen1 event {mod 1 poisson \[10.0 1024\]}"

# Stop at 100.0 seconds
$ns_ at 100.0 "finish $ns_ $f $nf"

proc finish {ns_ f nf} {
    close $f
    close $nf
    $ns_ halt
    delete $ns_
    puts "Simulation complete."
}

$ns_ run

