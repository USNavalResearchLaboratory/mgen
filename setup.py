from distutils.core import setup

# Note that the 'mgen' package has a dependency
# on the Protolib Python 'protokit' package and
# also requires that the 'mgen' binary be installed/
# located in the user's executable PATH.


# TBD - invoke the "protolib/setup.py" automatically?
# (good intern task to do this the right way)

setup(name='mgen',
      version='1.0',
      package_dir = {'' : 'src/python'},
      py_modules=['mgen']
)
    
