controlR
========

controlR is a [node][1] module for running commands in an external [R][2] process.

License
-------

controlR is copyright (c) 2016 Structured Data LLC and released under the MIT license.  
See source files for license details.

Rationale
---------

R is great for data processing, in particular because of the excellent libraries
developed by [the user community][3].  Once you have developed your data model, you
can run it through the R shell or GUI provided.  Beyond that, it's pretty easy to
embed R in a C/C++ application.  You can build a full-featured desktop application
or service this way.

Requiring either the R shell or a C++ host application can be a bit limiting, though.
Modern applications tend to use a variety of platforms, languages, and environments
to solve different problems.  Javascript, and in particular javascript through Node,
has become the de facto language for modern application development.  It's not for
everything; but it solves a lot of problems, provides a complex ecosystem of libraries
and developers, and (via [electron][6], among other platforms) supports desktop
applications as well as services.

Javascript is not for everything -- that's why we want to use R in the first place.
But node provides a great environment for building applications.  controlR is built to
be glue code that lets you build your application in javascript, your data model in
R, and connect the two.

Moreover because controlR runs R in an external process, you have benefits beyond
what you get by embedding in a C++ application -- for example, you can monitor
execution and kill off runaway processes; or you can start multiple instances and
run code in parallel.

The child process will run R's event loop, meaning it supports things like R's html
help server.  This is probably only useful for desktop applications.

One further note on rationale -- for our purposes we did not want to modify the R
source code in any way.  controlR binds against the R shared libraries (DLLs on
windows) at runtime.  This way we can guarantee fidelity with the standard R
interpreter, and building against updated versions of R is trivial.

What it is not
--------------

controlR is not designed for, nor is it suitable for, running a web service.  It is
designed to support a single client connection; and it adds no limitations on what
running R code does to the host system.  

R has full access to the system (subject to the host process' permissions) and the
interface imposes no security restrictions on top of this.  Therefore exposing the
R interface to outside users is a significant security risk, even if the host process
is running with minimal permissions.  

R processes maintain internal state.  If multiple clients connect to the same running
instance, each client will have access to and can modify that state.  For some purposes
this is immaterial; but for our purposes this is not desirable.  Therefore there is a
tight binding between a single client and a single R process.  

On the other hand, there is no reason you can't run multiple R processes at the same time,
and talk to them from a single client or from multiple logical clients.

Connection
----------

controlR consists of a javascript module, for use with node; and a standalone executable,
which acts as a host for R (via the shared library/dll).  Communication between node and
the R process runs over a domain socket (named pipe on Windows) or a TCP socket.  All
messages transferred between the processes are JSON formatted.

Interface
---------

controlR connects to and talks to a single R instance.  Within the interface there are
two separate "channels" for communication.  In the API these are generally referred to
as `exec` and `internal`.

The `internal` channel uses the embedded R interface.  This is generally what you want
if you want to execute some R code and get a result back.  For most purposes this is
sufficient to build an R application.  The `exec` channel talks to R through R's REPL
loop -- much as if you were using an R shell.  Why do this at all? to support debugging
and R's concept of a `browser` -- a window into executing code.  Without modifying R code,
this is the only way to support debugging.  We can also use it to build our R shell
(more on that later).

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

See the `js/` directory for API and event documentation.

Third-Party Dependencies
------------------------

controlR depends on R and node.  See build instructions.  controlR futher depends on
[libuv][4] and [nlohmann::json][5], used under license and included in source distributions.
See the individual projects for license details.  

Building
--------

Although the node module itself is pure javascript, you need standard node build tools to
build the child executable.  You also need R installed.  On linux, R must be built with
support for shared libraries.

To build, set an environment variable `R_HOME` pointing to the root of the R directory.
Then use `npm` or `node-gyp` to build.

```
> export R_HOME=/path/to/R-3.2.3
> npm install controlr
```

Example
-------

At runtime, you can either set an `R_HOME` environment variable or pass a value directly
to the initialization method.  Remember to escape backslashes in Windows paths.

```javascript

const ControlR = require( "controlr" )

var controlr = new ControlR();

controlr.init({
    rhome: "/path/to/R-3.2.3"
}).then( function(){
	console.info( "initialized OK" );
	return controlr.internal( "1+1" );
}).then( function(rslt){
	console.info( "result:", rslt );
	console.info( "shutting down" );
	return controlr.shutdown();
}).then( function(){
	console.info( "shutdown complete" );
}).catch( function(e){
	console.info( "error", e );
});

```

See Also
--------

[Rserve][13]

[1]: https://nodejs.org
[2]: https://www.r-project.org/
[3]: https://cran.r-project.org/
[4]: https://github.com/libuv/libuv
[5]: https://github.com/nlohmann/json
[6]: http://electron.atom.io/
[13]: https://rforge.net/Rserve/
