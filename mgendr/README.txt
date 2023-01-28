"mgendr" is an application based on the MGEN code base that provides a simple Graphical User Interface (GUI) for managing MGEN operation.  A log file can be set and and MGEN script can be loaded and run.  Note that most MGEN command-line options can also be used as commands in a script so the "mgendr" application, via scripting can be used to do almost anything the "mgen" command-line application can do.  The "mgendr" GUI also supports a "probe" feature that works as a sort of MGEN "ping" where a configurable MGEN flow can be sent to a remote "mgendr" destination and the destination will automatically feedback reporting information on the flow throughput, loss, and latency observed ny the destination to the "mgendr" sender.


BUILD:

The "mgendr" application uses the JUCE framework for its graphical user interface elements.  The JUCE source code can be retrieved from GitHub using:

git clone https://github.com/juce-framework/JUCE.git

JUCE provides a tool called Projucer that will generate build files for the supported Linux, Windows, MacOSX, iOS, and Android operating systems.  That tool needs to be built from the JUCE source (or otherwise obtained) and used to open the "mgen/mgendr/mgendr.jucer" file in the MGEN source tree.

In the Projucer application, you will need to set it "Global Paths" preferences to provide paths to the JUCE source code directory and modules directory.  The Projucer generates build files (Visual Studio, XCode, Android Studio, Linux Makefiles, etc) and includes a copy of the JUCE code along with references to, in this case, the "mgendr" source code files (The "mgendr.jucer" configuration has relative paths to the MGEN source files, etc).
