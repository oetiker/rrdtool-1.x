#!/usr/bin/env python3
"""Remove the installed files recorded for the wheel built by this tree."""

import csv
import errno
import os
import sys
import zipfile


def uninstall(target, wheel):
    target = os.path.abspath(target)
    with zipfile.ZipFile(wheel) as archive:
        record, = (name for name in archive.namelist()
                   if name.endswith('.dist-info/RECORD'))
    record = os.path.join(target, record)
    if not os.path.isfile(record):
        return

    with open(record, newline='', encoding='utf-8') as fh:
        paths = [os.path.abspath(os.path.join(target, row[0]))
                 for row in csv.reader(fh)]
    # Validate the complete record before removing anything. This binding
    # has no scripts outside the --target directory.
    if any(os.path.commonpath([target, path]) != target for path in paths):
        raise ValueError('installed RECORD contains a path outside ' + target)

    directories = set()
    for path in paths:
        try:
            os.unlink(path)
        except FileNotFoundError:
            pass
        parent = os.path.dirname(path)
        while parent != target:
            directories.add(parent)
            parent = os.path.dirname(parent)
    for directory in sorted(directories, key=len, reverse=True):
        try:
            os.rmdir(directory)
        except OSError as exc:
            if exc.errno not in (errno.ENOENT, errno.ENOTEMPTY, errno.EEXIST):
                raise


if __name__ == '__main__':
    uninstall(*sys.argv[1:])
