#!/usr/bin/env python
import os

try:
    from setuptools import setup, Extension
except ImportError:
    sys.exit('The setup requires setuptools.')

TOP_SRCDIR = os.environ.get('ABS_TOP_SRCDIR', '../..')
TOP_BUILDDIR = os.environ.get('ABS_TOP_BUILDDIR', '../..')

# package version
package_version = '0.1.10'


def main():
    module = Extension('rrdtool',
                       sources=['rrdtoolmodule.c'],
                       library_dirs=[os.path.join(TOP_BUILDDIR, 'src', '.libs')],
                       include_dirs=[os.path.join(TOP_BUILDDIR, 'src'),
                                     os.path.join(TOP_SRCDIR, 'src')],
                       libraries=['rrd'])

    kwargs = dict(
        name='rrdtool',
        version=package_version,
        description='Python bindings for rrdtool',
        keywords=['rrdtool'],
        author='Christian Kroeger, Hye-Shik Chang',
        author_email='commx@commx.ws',
        license='LGPL',
        url='https://github.com/commx/python-rrdtool',
        classifiers=['License :: OSI Approved',
                     'Operating System :: POSIX',
                     'Operating System :: Unix',
                     'Operating System :: MacOS',
                     'Programming Language :: C',
                     'Programming Language :: Python',
                     'Programming Language :: Python :: 2.7',
                     'Programming Language :: Python :: 3.3',
                     'Programming Language :: Python :: 3.4',
                     'Programming Language :: Python :: 3.5',
        ],
        ext_modules=[module],
        test_suite='tests'
    )

    setup(**kwargs)


if __name__ == '__main__':
    main()
