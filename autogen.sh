#!/bin/sh
PATH="/usr/pack/automake-1.11-to/amd64-linux-ubuntu8.04/bin:$PATH"
export PATH
autoreconf --force --install --verbose -I config -I m4
