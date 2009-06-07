#!/bin/sh
libtoolize
autoreconf --force --install --verbose -I config -I m4
