const ControlR = require( "../js/controlr.js")
const repl = require('repl');

var controlr = new ControlR();
controlr.on('console', function(msg){
    console.info(msg);
})

controlr.init({
    rhome: "/home/duncan/dev/R-3.2.3",
//    rhome: "e:\\editR\\R-3.2.3",
    debug: true
/*
}).then( function(a){

	console.info( "setting err");
	return controlr.exec( "options(error=dump.frames)");
*/
}).then( function(a){

	console.info( "executing crap");
	controlr.exec( "A SKAS AS");
	console.info( "done");
	
});

repl.start('> ').context.controlr = controlr;


