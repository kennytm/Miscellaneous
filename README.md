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

[machoizer.py](https://github.com/kennytm/Miscellaneous/blob/master/machoizer.py)
--------------

This is a small Python script that adds the necessary headers to turn a raw
binary file (e.g. the decrypted iBoot) into a Mach-O file. This is useful for
tools that cannot work with raw binary files, like `otool -tv` or the IDA Pro
demo.


[dump_stuff.py](https://github.com/kennytm/Miscellaneous/blob/master/dump_stuff.py)
----------------

This script is a collection of utilities to dump information from different 
libraries. Currently it supports dumping of CAAtom and UISound.

[CAAtom](http://iphonedevwiki.net/index.php?title=CAAtom) is an internal data
type in Core Animation which creates a mapping between strings and an integer
index. This optimizes string comparison operation over known strings since they
are already perfectly hashed. However, this poses a difficulty in
reverse-engineering because the relevant strings are all replaced with some
unrelated numbers. This script supports reading the table that defines the
mappings of the internal atoms.

UISound is a directory in iOS containing .caf files for system alert sounds.
These sounds are indiced by a constant number and can be used as the SoundID in
[AudioServices](http://iphonedevwiki.net/index.php/AudioServices) to play them.
This script supports interpreting the sound IDs and categories for these files.


[log_rename.idc](https://github.com/kennytm/Miscellaneous/blob/master/log_rename.idc)
----------------

Often executables or kernels are stripped, so guessing what a function does
would require heavy analysis of its content. Nevertheless, developers usually 
will leave a logging function which accepts `__FUNCTION__`, i.e. the function
name, as an input parameter. If such a function is found, the function names can
be assigned systematically.

The `log_rename.idc` script is written to take advantage of this. Once you have
identified any function that takes a C string function name as an input
parameter (via register r0 to r3), you could start this script to locate all
analyzed functions calling this. Then the script will coservatively try to
rename the function based on the input.

[ipsw_decrypt.py](https://github.com/kennytm/Miscellaneous/blob/master/ipsw_decrypt.py)
-----------------

This is a convenient script to extract, decrypt and decompress files in an IPSW
file in one pass. This script is **only** intended for decoding those files for
analysis, but not for building a jailbroken IPSW. The standard jailbreaking
software like PwnageTool or XPwn should be used instead for the latter purpose.

The script can perform the following:

 * Extract the encrypted files from an IPSW
 * Download decryption keys from <http://theiphonewiki.com/>
 * Perform AES decryption / VFDecrypt using these keys
 * Decompress the kernel into a Mach-O file, and iBootIm images into raw data.

This script requires the executables `openssl` (for AES decryption) and
[`vfdecrypt`](https://github.com/dra1nerdrake/VFDecrypt) (for decrypting the OS
DMG) to run. It also requires the [`lxml`](http://codespeak.net/lxml/installation.html)
module to be installed for HTML parsing.

