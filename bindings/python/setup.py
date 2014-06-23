#! /usr/bin/env python
#
# setup.py
#
# py-rrdtool distutil setup
#
# Author  : Hye-Shik Chang <perky@fallin.lv>
# Date    : $Date: 2003/02/14 02:38:16 $
# Created : 24 May 2002
#
# $Revision: 1.7 $
#
#  ==========================================================================
#  This file is part of py-rrdtool.
#
#  py-rrdtool is free software; you can redistribute it and/or modify
#  it under the terms of the GNU Lesser General Public License as published
#  by the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  py-rrdtool is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public License
#  along with Foobar; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

try:
    # Attempt to build using Distribute, which also supports bdist_wheel
    from setuptools import setup
    from setuptools.extension import Extension
except ImportError:
    from distutils.core import setup, Extension
import sys, os

TOP_SRCDIR = os.environ.get('ABS_TOP_SRCDIR', '../../src')
TOP_BUILDDIR = os.environ.get('ABS_TOP_BUILDDIR', '../../src')

setup(name = "py-rrdtool",
      version = "0.2.2",
      description = "Python Interface to RRDTool",
      author = "Hye-Shik Chang",
      author_email = "perky@fallin.lv",
      license = "LGPL",
      url = "http://oss.oetiker.ch/rrdtool",
      #packages = ['rrdtool'],
      ext_modules = [
          Extension(
            "rrdtoolmodule",
            ["rrdtoolmodule.c"],
            libraries=['rrd'],
            library_dirs=[ os.path.join(TOP_BUILDDIR, 'src', '.libs') ],
            include_dirs=[ os.path.join(TOP_BUILDDIR, 'src'),
                           os.path.join(TOP_SRCDIR, 'src') ],
          )
      ]
)
