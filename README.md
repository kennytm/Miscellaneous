Miscellaneous
=============

This repository contains stuff which would be helpful for jailbroken iOS
development.

[fixobjc2.idc](https://github.com/kennytm/Miscellaneous/blob/master/fixobjc2.idc)
--------------

This is a script to simply studying of Mach-O files on ARM architecture using
Objective-C ABI 2.0 for
[IDA Pro](http://en.wikipedia.org/wiki/Interactive_Disassembler) 5.5 and above.
Currently, the script mainly does the following:

 * Add comments to all selectors so it becomes clear which selector
   `_objc_msgSend` is using.
   
 * Check all Objective-C methods and create functions for them. This is
   particularly useful for files with symbols stripped because IDA Pro usually
   can't recognize those functions as code.
 
 * Add name to all ivars and classes.

dyld_decache
------------

Starting from iPhone OS 3.1, the individual libraries files supplied by the
system are smashed together into a giant cache file (`dyld_shared_cache_armvX`)
to improve performance. This makes development difficult when one only has the
IPSW but not the SDK (e.g. Apple TV 2G), because there is no file to link to or
reverse engineer.

`dyldcache.cc`, originally written by [D. Howett](http://blog.howett.net/?p=75),
was created to unpack files from the `dyld_shared_cache_armvX` file.
Unfortunately, this tool does not try to untangle the interconnection between
libraries in the cache, so each extracted file is over 20 MiB in size (as the
whole `__LINKEDIT` segment is duplicated) and often the file cannot be correctly
`class-dump`'ed (as some data are actually outside of that library).

`dyld_decache` is a complete rewrite of the tool to solve the above problems. It
correctly excludes irrelevant parts of the `__LINKEDIT` segments, and pulls in
sections which are placed outside of the file due to `dyld`'s optimization. As a
result, the generated files take only roughly 200 MiB totally in size instead of
over 4 GiB previously, and they can be correctly analyzed by `class-dump`.

The 64-bit `dyld_decache` for Mac OS X 10.6 can be downloaded from
<https://github.com/kennytm/Miscellaneous/downloads>. It is a command line tool,
the options are:

    Usage:
      dyld_decache [-p] [-o folder] [-f name [-f name] ...] path/to/dyld_shared_cache_armvX
    
    Options:
      -o folder : Extract files into 'folder'. Default to './libraries'
      -p        : Print the content of the cache file and exit.
      -f name   : Only extract the file with filename 'name', e.g. '-f UIKit' or
                  '-f liblockdown'. This option may be specified multiple times to
                  extract more than one file. If not specified, all files will be
                  extracted.

machoizer.py
------------

This is a small Python script that adds the necessary headers to turn a raw
binary file (e.g. the decrypted iBoot) into a Mach-O file. This is useful for
tools that cannot work with raw binary files, like `otool -tv` or the IDA Pro
demo.


