#Setup default settings
import os
env = Environment(CPPPATH=['src/os/inc','build/inc'],
                  CCFLAGS=[],
                  LIBS=[],
                  LIBPATH=[],
                  LINKFLAGS = [],
                  CPPDEFINES=[],
                  ENV = {'PATH' : os.environ['PATH']})
env.Decider('MD5-timestamp') #Speeds up scons.

env['PROJECT'] = 'posix'
env['CPPDEFINES'] += ['_ix86_']


env['CCFLAGS']   += ['-g3','-fPIC']
env['CCFLAGS']   += ['-O2']
#env['CCFLAGS']   += ['-m64']
#env['CCFLAGS']   += ['--coverage']
#env['LINKFLAGS'] += ['--coverage']

#Overload the enviorment variables....
env["CC"] = os.getenv("CC") or env["CC"]
env["CXX"] = os.getenv("CXX") or env["CXX"]
env["ENV"].update(x for x in os.environ.items() if x[0].startswith("CCC_"))
#env["CC"] = "scan-build gcc"  
#env["CXX"] = "scan-build g++" 
#env["CC"] = "clang"

platform = ARGUMENTS.get('OS', Platform())
defines = ''

if platform.__str__() == 'posix':
    pass
elif platform.__str__() == 'darwin':
    pass
else:
    pass

#Compile Operating system.
env.Append(CPPDEFINES=defines)

env['LIBS'] += ['c']
env['STATIC_AND_SHARED_OBJECTS_ARE_THE_SAME']=1

src = []
src += Glob('src/os/' + env['PROJECT'] + '/*.c')
src += Glob('src/os/' + env['PROJECT'] + '/*.cpp')
src += ['src/bsp/pc-linux/src/bsp_timer.c','src/bsp/pc-linux/src/bsp_voltab.c' ]
env['CPPPATH'] += ['src/os/' + env['PROJECT']] 


platform = ARGUMENTS.get('OS', Platform())

print src
env.StaticLibrary('osal',src)
env.SharedLibrary('osal',src)



