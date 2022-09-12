import platform
from distutils.core import setup, Extension

# Note that the 'mgen' package has a dependency
# on the Protolib Python 'protokit' package and
# also requires that the 'mgen' binary be installed/
# located in the user's executable PATH.

PYTHON = "src/python/"
COMMON = "src/common/"

srcFiles = [COMMON + 'gpsPub.cpp', PYTHON + 'gpsPub_wrap.c']
sys_libs = ['_gpsPub']

system = platform.system().lower()
sys_macros = [('HAVE_ASSERT',None), ('HAVE_IPV6',None), ('PROTO_DEBUG', None)]

if system == 'darwin':
    sys_libs.append('resolv') 

# TBD - invoke the "protolib/setup.py" automatically?
# (good intern task to do this the right way)

setup(name='mgen',
      version='1.0',
      package_dir = {'' : 'src/python'},
      py_modules=['mgen']
)
    
setup(name='gpsPub',
      version='1.0',
      ext_modules = [Extension('_gpsPub',
                               srcFiles,
                               include_dirs = ['./include'],
                               define_macros=sys_macros,
#                               libraries = sys_libs,
                               library_dirs = ['./lib'])])
