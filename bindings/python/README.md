python-rrdtool
==============

Python bindings for [RRDtool](http://oss.oetiker.ch/rrdtool) with a native C extension.

Supported Python versions: 2.6+, 3.3+.

The bindings are based on the code of the original Python 2 bindings for rrdtool by Hye-Shik Chang, which are currently shipped as official bindings with rrdtool.

Installation
------------

The easy way:

    # pip install rrdtool

**Note:** This requires rrdtool and it's development files (headers, libraries, dependencies) to be installed.

In case you'd like to build the module on your own, you can obtain a copy of the repository and run `python setup.py install` in it's destination folder to build the native C extension.

Usage
-----

```python
import rrdtool

# Create Round Robin Database
rrdtool.create('test.rrd', '--start', 'now', '--step', '300', 'RRA:AVERAGE:0.5:1:1200', 'DS:temp:GAUGE:600:-273:5000')

# Feed updates to the RRD
rrdtool.update('test.rrd', 'N:32')
```

Documentation
-------------

You can find the latest documentation for this project at http://pythonhosted.org/rrdtool.