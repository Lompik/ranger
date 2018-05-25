from __future__ import print_function

import errno
import os

from ctypes import *
from sys import platform, maxsize
import sys
sys.path.append("/tmp")
import _scandir


def scandir(path):
    return _scandir.par_dir_stat(path)

def remove_dots(dirlist, top):
    for d in [".", ".."]:
        if dirlist.get(os.path.join(top, d)):
            del dirlist[os.path.join(top, d)]


DT_FIFO = 1
DT_CHR = 2
DT_DIR = 4
DT_BLK = 6
DT_REG = 8
DT_LNK = 10
DT_SOCK = 12
DT_WHT = 14

def _walk(top, topdown=True, onerror=None, followlinks=False):
    """Like Python 3.5's implementation of os.walk() -- faster than
    the pre-Python 3.5 version as it uses scandir() internally.
    """
    dirs = []
    nondirs = []

    # We may not have read permission for top, in which case we can't
    # get a list of the files the directory contains.  os.walk
    # always suppressed the exception then, rather than blow up for a
    # minor reason when (say) a thousand readable directories are still
    # left to visit.  That logic is copied here.
    try:
        scandir_it = _scandir.par_dir_stat(top)
        remove_dots(scandir_it, top)
    except OSError as error:
        if onerror is not None:
            onerror(error)
        return

    for entry in scandir_it.values():
        try:
            is_dir = entry.is_dir()
        except OSError:
            # If is_dir() raises an OSError, consider that the entry is not
            # a directory, same behaviour than os.path.isdir().
            is_dir = False

        if is_dir:
            dirs.append(entry)
        else:
            nondirs.append(entry)

        if not topdown and is_dir:
            # Bottom-up: recurse into sub-directory, but exclude symlinks to
            # directories if followlinks is False
            if followlinks:
                walk_into = True
            else:
                try:
                    is_symlink = entry.is_symlink()
                except OSError:
                    # If is_symlink() raises an OSError, consider that the
                    # entry is not a symbolic link, same behaviour than
                    # os.path.islink().
                    is_symlink = False
                walk_into = not is_symlink

            if walk_into:
                for entry in _walk(entry.path, topdown, onerror, followlinks):
                    yield entry

    # Yield before recursion if going top down
    if topdown:
        yield top, dirs, nondirs

        # Recurse into sub-directories
        for name in dirs:
            new_path = os.path.join(top, name.path)
            # Issue #23605: os.path.islink() is used instead of caching
            # entry.is_symlink() result during the loop on os.scandir() because
            # the caller can replace the directory entry during the "yield"
            # above.
            if followlinks or not os.path.islink(new_path):
                for entry in _walk(new_path, topdown, onerror, followlinks):
                    yield entry
    else:
        # Yield after recursion if going bottom up
        yield top, dirs, nondirs

def walklevel(some_dir, level):
    some_dir = some_dir.rstrip(os.path.sep)
    followlinks = True if level > 0 else False
    assert os.path.isdir(some_dir)
    num_sep = some_dir.count(os.path.sep)
    for root, dirs, files in _walk(some_dir, followlinks=followlinks):
        yield root, dirs, files
        num_sep_this = root.count(os.path.sep)
        if level != -1 and num_sep + level <= num_sep_this:
            del dirs[:]


def mtimelevel(path, level):
    return _scandir.mtimelevel(path, level)

def walklevel_py(some_dir, level):
    some_dir = some_dir.rstrip(os.path.sep)
    followlinks = True if level > 0 else False
    assert os.path.isdir(some_dir)
    num_sep = some_dir.count(os.path.sep)
    for root, dirs, files in os.walk(some_dir, followlinks=followlinks):
        yield root, dirs, files
        num_sep_this = root.count(os.path.sep)
        if level != -1 and num_sep + level <= num_sep_this:
            del dirs[:]


def mtimelevel_py(path, level):
    mtime = os.stat(path).st_mtime
    for dirpath, dirnames, _ in walklevel_py(path, level):
        dirlist = [os.path.join("/", dirpath, d) for d in dirnames
                   if level == -1 or dirpath.count(os.path.sep) - path.count(os.path.sep) <= level]
        mtime = max(mtime, max([-1] + [os.stat(d).st_mtime for d in dirlist]))
    return mtime
