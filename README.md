            MGEN Version 5.x

This directory contains source code for the NRL Multi-Generator
(MGEN) Test Tool Set Version 5.x.  MGEN Version 5 is based on 
MGEN 4.2b6.  The transport classes have been abstracted and new
features have been added.

Primary new features include:

1)  Support for the TCP protocol. 
2)  New pattern options JITTER and CLONE.
3)  Transport buffering has been added (QUEUE option).
4)  User defined payload can be specified with the DATA option.
5)  Message COUNT support. (Concrete limit for messages sent per flow).
6)  Modifications to the log file.  (New TCP messages and additional
    message content).
7)  Works with the RAPR toolkit. <http://cs.itd.nrl.navy.mil/work/rapr/index.php>
8)  Compile time option to randomly fill payload buffer.
9)  OS can now choose src port.
10) Command line option to log in localtime or gmtime.
11) Includes SO_BROADCAST socket option provided by Erik Auerswald. 
12) Includes bug fix for compiling under Visual Studio 6.0 (submitted
    by Kevin Wambsganz)
13) Includes bug fix for retaining src port upon flow MOD command.

Please refer to the Mgen User's Guide for more information on these
and other features at <http://pf.itd.nrl.navy.mil/mgen/mgen.html> or
in the distribution.

FILES AND DIRECTORIES:

README.TXT  - this file.

include     - Include files

src         - Directory with cross-platform source code files
              src/common contains mgen src code files.  
              src/sim contains ns and opnet src code files.

doc         - Documentation directory.  
              mgen.[HTML|PDF] (MGEN User's and Reference Guide)
              mgen-tech.[HTML|PDF] (MGEN Technical Documentation)
              example.mgn (Example MGEN script file)

protolib    - NRL Protolib source tree (See
              <http:://protolib.pf.itd.nrl.navy.mil>)

makefiles   - Directory with os-specific Makefiles.
              Linux specific makefiles are at the top level,
              win32 and wince subdirectories contains windows
              build files.
