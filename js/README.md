API
===

 * busy()

   Return the busy state of the R process.  R is busy when it 
   is executing a command.  
   
   returns: boolean

 * exec( commands )

   Execute a command on the `exec` channel (see the main README.md for details).  
   
   commands: string or array of strings  
   returns: promise
 
 * internal( commands )

   Execute a command on the `internal` channel (see the main README.md for details).

   commands: string or array of strings   
   returns: promise

 * queued_exec( commands, [key] )

   Same as `exec` but uses an execution queue so that it will not fail if 
   R is busy.  If R isn't busy, it will execute immediately.  If R is busy,
   it will be queued in line.

   Sometimes you send a command but then send another command which would
   supercede the first.  For example, if you are resizing a shell based on 
   user action, you might send multiple commands to set an option.  Using the
   `key` parameter will remove any previous commands that have the same `key`
   before queueing up this command.

   commands: string or array of strings  
   key (optional): a key used to identify this command  
   returns: promise
 
 * queued_internal( commands, key )

   Queued execution on the `internal` channel. 

   commands: string or array of strings  
   key (optional): a key used to identify this command  
   returns: promise

 * send_break()

   set the break flag (equivalent to sending a SIGINT on linux),
   which will break R execution, although not necessarily immediately.

   returns: nothing
 
 * init( options )

   options: an object, with the following parameters (all optional):

   * connection_type: string. either "tcp" or "domain", the default
   * port: number. the desired port if using tcp
   * debug: boolean. enables additional console logging
   * rhome: string.  path to the R directory, if not using an environment variable
   * permissive: boolean.  as noted below, the module supports user-defined messages generated 
     by R and passed to listeners.  setting this flag enables any message to pass through, otherwise
	 messages with unknown types may be filtered.
   
   returns: promise
 
 * shutdown()
 
   shutdown the child process and clean up
 
   returns: promise
 

Events
======

ControlR implements node's `eventemitter` interface.  Some events are implemented
by the module, but it also supports custom events via R callbacks (used for things
like graphics in our JS client module).  Only standard messages are documented here.

To subscribe to a particular event, use `on`.  for example, to subscribe to console
messages, use

```javascript
controlr.on( "console", function(data){ /* do something */ })
```
see node documentation for more on eventemitter.

Standard Events
---------------

 * state-change

   broadcasts events when the system starts or finishes executing a command,
   essentially the same as polling the `busy()` function.

 * system
 
   system status messages

 * console

   Messages that R would normally print to the console.  If you use the `exec` 
   channel, results of any R call will generally be printed instead of returned.
 

