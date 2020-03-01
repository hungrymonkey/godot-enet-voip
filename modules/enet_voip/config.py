def can_build(env, platform):
    return True

def configure(env):
	env.ParseConfig('pkg-config capnp --cflags --libs')
	env.Append(LIBS=['kj'])