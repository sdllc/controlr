
const fs = require( "fs" );
const os = require( "os");
const net = require( "net" );
const path = require( "path" );
const events = require( "events" );
const child_process = require( "child_process" );

var rhome = "/home/duncan/dev/R-3.2.3";
var file = "/home/duncan/dev/modernr/build/Release/controlr";
var socket = "r.3790900";

var opts = {
    env: {
        R_HOME: rhome,
        LD_LIBRARY_PATH: path.join( rhome, 'lib')
    }
};
    
var proc = child_process.spawn( file, [socket], opts );
    
proc.stdout.on('data', (data) => {
    console.log(`stdout: ${data}`);
});

proc.stderr.on('data', (data) => {
    console.log(`stderr: ${data}`);
});

proc.on('close', (code) => {
    console.log(`child process exited with code ${code}`);
});

