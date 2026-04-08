# $Id: extconf.rb,v 1.2 2001/11/28 18:30:16 miles Exp $
# Lost ticket pays maximum rate.

require 'mkmf'

if ENV['ABS_TOP_BUILDDIR']
   ABS_TOP_BUILDDIR = ENV['ABS_TOP_BUILDDIR'] || '../..'
   ABS_TOP_SRCDIR = ENV['ABS_TOP_SRCDIR'] || '../..'

   if /linux/ =~ RUBY_PLATFORM
      $LDFLAGS += ' -Wl,--rpath -Wl,$(EPREFIX)/lib'
   elsif /solaris/ =~ RUBY_PLATFORM
      $LDFLAGS += ' -R$(EPREFIX)/lib'
   elsif /hpux/ =~ RUBY_PLATFORM
      $LDFLAGS += ' +b$(EPREFIX)/lib'
   elsif /aix/ =~ RUBY_PLATFORM
      $LDFLAGS += ' -blibpath:$(EPREFIX)/lib'
   end

   dir_config("rrd", ["#{ABS_TOP_BUILDDIR}/src", "#{ABS_TOP_SRCDIR}/src"], "#{ABS_TOP_BUILDDIR}/src/.libs")
else
   # standalone build: use pkg-config to find system-installed librrd
   unless pkg_config('librrd')
      abort "pkg-config could not find librrd. Is rrdtool installed?\n" \
            "Try: brew install rrdtool (macOS) or apt install librrd-dev (Debian/Ubuntu)"
   end
end

have_library("rrd", "rrd_create")
have_func("rb_ext_ractor_safe", "ruby.h")
create_makefile("RRD")
