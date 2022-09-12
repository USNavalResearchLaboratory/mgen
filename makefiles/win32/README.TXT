              MGEN 5.x for WIN32

This directory contains Visual Studio solution and project files for
building MGEN 5.x.  

Currently VC++ 6.0 and Visual Studio 2008 Express are supported.  

Microsoft's Platform SDK is required and the path should be defined in
the Projects and Solutions Section of the Options menu (off the Tools
menu) in Visual Studio.

Note that the MGEN project is dependent on the PROTOLIB project.  

If Visual C++ has not been installed on the target computer,
Microsoft's Visual C++ 2008 Redistributable Package (X86) should be
installed (vcredist_x86.exe).  This package installs runtime
components of Visual C++ Libraries required to run applications
developed with Visual C++.

NOTES:

1) Performance may be further optimized by undefining the
PROTO_DEBUG preprocessor definition at a cost of losing
detailed debug messages.
