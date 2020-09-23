            MGEN Version 5.x

This directory contains source code for the NRL Multi-Generator
(MGEN) Test Tool Set Version 5.x.  MGEN Version 5 is based on 
MGEN 4.2b6.  The transport classes have been abstracted and new
features have been added.

Requires Protolib:
    https://www.nrl.navy.mil/itd/ncs/products/protolib
    https://github.com/USNavalResearchLaboratory/protolib
    
    The MGEN build expects the "protolib" source tree (or a symbolic link to it) to
    be located in the top level of the "mgen" source tree.  For example, to 
    download and build on Linux:
    
    git clone https://github.com/USNavalResearchLaboratory/mgen.git
    cd mgen
    git submodule update --init
    cd makefiles
    make -f Makefile.linux

Primary new features included in 5.02c

1) includes/is compiled with protolib-3.0b1
2) IPV6 on linux is fixed
3) DF fragmentation bit on|off added
4) uniform random message size supporte e.g. <pattern> [<rate> <sizeMin:sizeMax>]
5) assorted bug fixes

Primary new features included 5.02b:

1)  Support for the TCP protocol. 
2)  New pattern options JITTER and CLONE.
3)  Transport buffering has been added (QUEUE option).
4)  User defined payload can be specified with the DATA option.
5)  Message COUNT support. (Concrete limit for messages sent per flow).
6)  Modifications to the log file.  (New TCP messages and additional
    message content).
7)  Works with the RAPR toolkit. <https://www.nrl.navy.mil/itd/ncs/products/rapr>
8)  Compile time option to randomly fill payload buffer.
9)  OS can now choose src port.
10) Command line option to log in localtime or gmtime.
11) Includes SO_BROADCAST socket option provided by Erik Auerswald. 
12) Includes bug fix for compiling under Visual Studio 6.0 (submitted
    by Kevin Wambsganz)
13) Includes bug fix for retaining src port upon flow MOD command.

Please refer to the Mgen User's Guide for more information on these
and other features at <https://www.nrl.navy.mil/itd/ncs/products/mgen> or
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
              <https://www.nrl.navy.mil/itd/ncs/products/protolib>)

makefiles   - Directory with os-specific Makefiles.
              Linux specific makefiles are at the top level,
              win32 and wince subdirectories contains windows
              build files.
              
setup.py    - Python installation script for installing the Python 'mgen'
              package that provides for Python-based control and monitoring
              of MGEN.  This package assumes the 'mgen' binary is 
              installed/located in the executable "path" (e.g., "/usr/local/bin")
              This package also requires that the Protolib (see above)
              'protokit' Python package has also been installed.  There is a
              similar 'setup.py' script in the "protolib" source tree.
