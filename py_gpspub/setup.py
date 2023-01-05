import platform
from distutils.core import setup, Extension

# Note that the 'mgen' package has a dependency
# on the Protolib Python 'protokit' package and
# also requires that the 'mgen' binary be installed/
# located in the user's executable PATH.

srcFiles = ['src/common/gpsPub.cpp', 'src/python/gpsPub_wrap.c']
sys_libs = ['_gpsPub']

system = platform.system().lower()
sys_macros = [('HAVE_ASSERT',None), ('HAVE_IPV6',None), ('PROTO_DEBUG', None)]

if system == 'darwin':
    sys_libs.append('resolv') 

setup(name='gpsPub',
      version='1.0',
      ext_modules = [Extension('_gpsPub',
                               srcFiles,
                               include_dirs = ['../include'],
                               define_macros=sys_macros,
#                               libraries = sys_libs,
                               library_dirs = ['../lib'])])
