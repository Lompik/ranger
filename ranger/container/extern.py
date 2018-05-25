from __future__ import print_function

import errno
import os

from ctypes import *
from sys import platform, maxsize

is_64bits = maxsize > 2**32

test = cdll.LoadLibrary("/tmp/libtest.so")

class Stats(Structure):
    _fields_ = [('filename', c_char_p),
                ('st_dev', c_ulong),
                ('st_ino', c_ulong),
                ('st_uid', c_uint),
                ('st_gid', c_uint),
                ('st_mode', c_ushort),
                ('st_nlink', c_uint),
                ('st_size', c_ulonglong),
                ("s_atime", c_double),
                ("s_ctime", c_double),
                ("s_mtime", c_double)]


c_listFiles = test.listFiles
c_listFiles.argtypes = [c_char_p, (POINTER(POINTER(Stats)))]
c_listFiles.restype = c_int
c_release = test.release
c_release.argtypes = [(POINTER(POINTER(Stats)))]

# sts = (POINTER(Stats))()
# imax = listFiles(path, byref(sts))

def convert_to_dict(el):
    res = {}
    for k, typ in el._fields_:
        val = getattr(el, k)
        if typ == c_char_p:
            try:
                val = val.decode("utf-8")
            except UnicodeDecodeError:
                continue
        res.update({k: val})

    if res.get("filename", None):
        return res
    return {}

def listFiles_py(path):
    sts = (POINTER(Stats))()
    imax = c_listFiles(c_char_p(path.encode("utf-8")), byref(sts))
    res = []
    for i in range(imax):
        el = convert_to_dict(sts[i])
        if el:
            res.append(el)
    c_release(sts, imax)
    return res

from os import stat_result
def scandir(path):
    res = {}
    for path in listFiles_py(path):
        stat = stat_result((path["st_mode"],
                           path["st_ino"],
                           path["st_dev"],
                           path["st_nlink"],
                           path["st_uid"],
                           path["st_gid"],
                           path["st_size"],
                           path["s_atime"],
                           path["s_mtime"],
                           path["s_ctime"]))
        res.update({path["filename"]: stat})

    return res

import sys
sys.path.append("/tmp")
import _scandir
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
        for d in [".", ".."]:
            if scandir_it.get(os.path.join(top, d)):
                del scandir_it[os.path.join(top, d)]
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
