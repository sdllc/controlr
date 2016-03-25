ControlR
========

ControlR is a [node][1] module for starting, running, and executing commands in, an 
external [R][2] process.

License
-------

ControlR is copyright (c) 2016 Structured Data LLC and released under the MIT license.
See source files for license details.

Rationale
---------

R is great for data processing, in particular because of the excellent libraries 
developed by [the user community][3].  

What it is not
--------------

ControlR is not designed for, nor is it suitable for, running a web service.  It is
designed to support a single client connection.  

R processes maintain internal state.  If multiple clients connect to the same running
instance, each client will have access to and can modify that state.  For some purposes 
this is immaterial; but for our purposes this is not desirable.  Therefore there is a 
tight binding between a single client and a single R process.  

On the other hand, there is no reason you can't run multiple R processes at the same time, 
and talk to them from multiple individual clients.

Connection
----------

ControlR consists of a javascript module, for use with node; and a standalone executable,
which acts as a host for R (via the shared library/dll).  Communication between node and
the R process runs over a domain socket (named pipe on Windows) or a TCP socket.  All 
messages transferred between the processes are JSON formatted.

Interface
---------

ControlR connects to and talks to a single R instance.  Within the interface there are 
two separate "channels" for communication.  In the API these are generally referred to
as `exec` and `internal`, with some variants supporting delayed execution.

The `internal` channel uses the embedded R interface.  This is generally what you want
if you want to execute some R code and get a result back.  For most purposes this is 
sufficient to build an R application.

The `exec` channel talks to R through R's REPL loop -- much as if you were using an R 
shell.  Why do this at all? to support debugging and R's concept of a `browser` -- a
window into executing code.  Without modifying R code, this is the only way to support
debugging.  We can also use it to build our R shell (more on that later). 

So the `internal` channel executes code and returns a result, as a javascript object 
(JSON on the wire).  The `exec` channel executes code in a shell context, and (possibly)
prints results to the output console.  Anything R wishes to print to the output console
is sent to the client as a javascript message (again, JSON on the wire), using node's 
eventemitter interface.

R is single-threaded, and can run only one operation at a time.  State is maintained by
the module, which enforces linear execution.  State my be polled via the `busy()` method,
and the module broadcasts state change events.  

Calls to `exec` or `internal` will fail if another call is in process.  The module
provides `queued_exec` and `queued_internal` methods which will wait for a change in state
and then execute.

See Also
--------

[Rserve][13]


[1]: https://nodejs.org
[2]: https://www.r-project.org/
[3]: https://cran.r-project.org/
[3]: https://rforge.net/Rserve/
