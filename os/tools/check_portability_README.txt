Portability Check Script
========================

With Exanodes having to run on several platforms (Linux and Windows,
currently), we need to keep the code as portable as possible. To do
so, we chose to write an "OS library" that is a wrapper around the
most common system operations and which offers the same behaviour on
both Linux and Windows.

All Exanodes code must use the functions defined in this OS
library. As of now, most code does so, but there are still occurrences
of "standard" functions or headers that should really be replaced by
their OS library equivalent. Also, even if one is careful, one may
slip sometimes and write code using the forbidden standard functions.

Hence we need a way to automatically check that the code we write is
indeed portable and warn when it isn't.

This is the goal of the 'check_portability' script, which can be used
standalone or as part of the build chain. Its principles and usage is
explained below.


Source Annotations
------------------

Wrapper functions in the OS library may wrap different functions on
Linux and Windows. Thus we need a way to map an OS library function to
one or several platform-specific functions and keep tightly coupled
the code and this mapping.

This could be done by adding annotations in the style of GCC
attributes. This was the initial solution but was dropped in favor of
a better solution suggested by Marc: annotations are custom Doxygen
tags in the comment of the OS library's function prototypes. This has
two advantages:

  (1) Tools parsing C source aren't broken by the added annotations
      (which could be the case with GCC-style attributes).

  (2) The annotations are recognized by Doxygen and the documentation
      generated for each OS library function will mention the
      platform-specific function(s) replaced by that function.


Syntax:

  The syntax follows Doxygen's tag convention:

    @os_replace{<platform>, <function>, ...}

  where <function>, ... is a list of functions on platform <platform>
  that must not be used and must instead be replaced by the OS library
  function considered.

  Supported platforms are "Linux" and "Windows".


Caveat:

  Doxygen is not able to handle aliases with an arbitrary number of
  parameters and the @os_replace alias allows at most 4 functions.

  To circumvent this limitation, one should add an @os_replace alias
  with greater arity, or just add several @os_replace in the comment.


Examples:

  * The OS library function os_sleep() replaces sleep() on Linux and
    SleepEx() on Windows. Its doxygen comment contains the following
    annotations:

      @os_replace{Linux, sleep}
      @os_replace{Windows, SleepEx}

  * The os_rename() function replaces one function on Linux and
    several functions on Windows:

      @os_replace{Linux, rename}
      @os_replace{Windows, MoveFile, MoveFileEx, MoveFileTransacted}


How It Works
------------

The script can be used standalone from the commandline, or as part of
the build process. Type "check_portability -h" to get usage help.

The script parses the files given on its commandline and, when it
finds forbidden functions, it prints error messages that tell which
function(s) from the OS library should be used instead.

The error messages are printed in the same format as GCC's error
messages so that users can use the script from within their favorite
IDE (more on that later).

Note that, when run for the first time, the script will begin by
parsing the OS library headers located in os/include/ and will create
a cache containing the mapping from platform-specific, forbidden
functions to their replacement counterpart (in the OS library).
Subsequently, if the cache is up to date, the script will just read it
and check the files given on its commandline. This speeds up the
checking quite a bit.

On the doxygen side, there is nothing worth mentioning: Doxygen just
recognizes the new @os_replace tag and generates the documentation
accordingly.


Build Chain Integration
-----------------------

Requirement: check_portability must be in a directory of your PATH and
executable.

The script can act as a compiler wrapper. This allows one to use it in
the build chain as if it were an actual compiler and get both the
regular compiler errors *and* the portability errors.

To do so, CC has been redefined in Exanodes' configure.in:

  CC="check_portability -C $CC"

An option has also been added to disable the portability checks
(enabled by default):

  --without-portability-check

Note: The script supports out-of-tree compilation.

(The heuristic is to look for a Makefile in the current directory. If
there is no Makefile or the Makefile has no "top_srcdir" variable, the
script assumes it is being run from a subdirectory of the top source
directory. In both cases, the script walks up the path until it finds
an Exanodes spec file.)


Example:

  * Compiling pre-userspace Examsg with portability check enabled, the
    following error message (among others) would be issued:

      network.c:477: error: function rand() is not portable,
                            use os_get_random_bytes() instead

      network.c:1793: error: function sleep() is not portable,
                             use os_sleep() instead

    (The messages have been wrapped to improve readability)


IDE Integration
---------------

Requirement: check_portability must be in a directory of your PATH and
executable.

If portability checks are enabled in the build chain, one's IDE will
transparently report both the compilation errors per se and the
portability errors. And since error messages have the same format as a
compiler's (GCC), one can browse the portability errors reported by
the script just like one does with errors reported by a compiler.

What's more, Emacs people can use the provided tiny Elisp script
called check-portability.el: M-x check-portability runs the
portability check on the current buffer and reports portability errors
in a compilation window (thus one can then browse these errors with
M-g-n and M-g-p).
