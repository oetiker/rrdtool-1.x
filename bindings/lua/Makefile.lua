-- min version
local min_major, min_minor = 5, 1
local major, minor = string.match(_VERSION, 'Lua (%d+)\.(%d*)')

if (tonumber(major)  < min_major or
    tonumber(major) == min_major and tonumber(minor) < min_minor) then
  error(string.format(
       '\n\n*** Lua rrdtool module requires Lua %d.%d or greater. ***\n',
       min_major, min_minor))
  os.exit(1)
end
local lua_version = major .. '.' .. minor

local options = arg[1]
if options then
  io.write(string.gsub(options, ' (%S-=)', '\n%1'), '\n\n')
end

io.stdout:write([[
T= rrd
# Version
LIB_VERSION=0.0.8
LUA_VERSION=]],major, '.',minor,[[


# set lua include, lib and C installation dirs
PKG_CONFIG=$(firstword $(shell which pkg-config))
ifeq (pkg-config,$(findstring pkg-config,$(PKG_CONFIG)))
  LUA_LIBDIR=$(shell pkg-config --variable=INSTALL_CMOD lua$(LUA_VERSION))
  ifeq (,$(LUA_LIBDIR))
    $(warning *** couldn't find lua$(LUA_VERSION).pc)
  else
    LUA_CFLAGS=$(shell pkg-config --cflags lua$(LUA_VERSION) 2>/dev/null)
    LUA_LFLAGS=$(shell pkg-config --libs lua$(LUA_VERSION) 2>/dev/null)
  endif
else
  $(warning couldn't find pkg-config)
endif

ifeq (,$(LUA_LIBDIR))
    $(warning *** setting Lua dirs to defaults in src package)
    LUA_CFLAGS=-I/usr/local/include -I/usr/local/include/lua
    LUA_LFLAGS=-L/usr/local/lib/lua/$(LUA_VERSION) -llua
    LUA_LIBDIR=/usr/local/lib/lua/$(LUA_VERSION)
endif

]])

-- overwrite global LUA_LIBDIR if default lib is set
if lib then
  io.stdout:write([[
# override LUA_LIBDIR for site install
LUA_LIBDIR=]],lib,[[/$(LUA_VERSION)
]])
end

io.stdout:write([[

# OS dependent
LIB_EXT= .so

# if this "autoconf" doesn't work for you, set LIB_OPTION for shared
# object manually.
LD=$(shell ld -V -o /dev/null 2>&1)
ifneq (,$(findstring Solaris,$(LD)))
 # Solaris - tested with 2.6, gcc 2.95.3 20010315 and Solaris ld
 LIB_OPTION= -G -dy
else
 ifneq (,$(findstring GNU,$(LD)))
  # GNU ld
  LIB_OPTION= -shared -dy
 else
  $(error couldn't identify your ld. Please set the shared option manually)
 endif
endif

RRD_CFLAGS=-I../../src/
RRD_LIB_DIR=-L../../src/.libs -lrrd

# Choose the PIC option
# safest, works on most systems
PIC=-fPIC
# probably faster, but may not work on your system
#PIC=-fpic

# Compilation directives
OPTIONS= -O3 -Wall ${PIC} -fomit-frame-pointer -pedantic-errors -W -Waggregate-return -Wcast-align -Wmissing-prototypes -Wnested-externs -Wshadow -Wwrite-strings
LIBS= $(RRD_LIB_DIR) $(LUA_LFLAGS) -lm
CFLAGS= $(OPTIONS) $(LUA_CFLAGS) $(RRD_CFLAGS) -DLIB_VERSION=\"$(LIB_VERSION)\"
#CC= gcc

LIBNAME= $T-$(LIB_VERSION)$(LIB_EXT)

SRCS= rrdlua.c
OBJS= rrdlua.o

all: $(LIBNAME)

lib: $(LIBNAME)

*.o:	*.c

$(LIBNAME): $(OBJS)
	$(CC) $(CFLAGS) $(LIB_OPTION) $(OBJS) $(LIBS) -o $(LIBNAME)

install: $(LIBNAME)
	mkdir -p $(LUA_LIBDIR)
	cp $(LIBNAME) $(LUA_LIBDIR)
	strip $(LUA_LIBDIR)/$(LIBNAME)
	(cd $(LUA_LIBDIR) ; rm -f $T$(LIB_EXT) ; ln -fs $(LIBNAME) $T$(LIB_EXT))
	$(POD2MAN) --release=$(VERSION) --center=RRDLua --section=3 rrdlua.pod > $(PREFIX)/man/man3/rrdlua.3

test: $(LIBNAME)
	ln -sf $(LIBNAME) rrd.so
	lua test.lua

clean:
	rm -f $L $(LIBNAME) $(OBJS) *.so *.rrd *.xml *.png *~
]])

