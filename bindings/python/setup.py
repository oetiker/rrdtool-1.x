#!/usr/bin/env python
try:
    from setuptools import setup
    from setuptools.extension import Extension
except ImportError:
    from distutils.core import setup, Extension

TOP_SRCDIR = os.environ.get('ABS_TOP_SRCDIR', '../..')
TOP_BUILDDIR = os.environ.get('ABS_TOP_BUILDDIR', '../..')


def main():
    module = Extension('rrdtool',
                       sources=['rrdtoolmodule.c'],
                       include_dirs=[os.path.join(TOP_BUILDDIR, 'src', '.libs')],
                       library_dirs=[os.path.join(TOP_BUILDDIR, 'src'),
                                     os.path.join(TOP_SRCDIR, 'src')],
                       libraries=['rrd'])

    kwargs = dict(
        name='rrdtool',
        version='0.1.6',
        description='Python bindings for rrdtool',
        keywords=['rrdtool'],
        author='Christian Kroeger, Hye-Shik Chang',
        author_email='commx@commx.ws',
        license='LGPL',
        url='https://github.com/commx/python-rrdtool',
        ext_modules=[module]
    )

    setup(**kwargs)


if __name__ == '__main__':
    main()
