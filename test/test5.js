const ControlR = require( "../js/controlr.js")
const repl = require('repl');

var controlr = new ControlR();
controlr.on('console', function(msg){
    console.info(msg);
})

controlr.init({
//    rhome: "/home/duncan/dev/R-3.2.3",
    rhome: "e:\\editR\\R-3.2.3",
    debug: true
}).then( function(a){

	console.info( "init ok?" );

//	console.info( "setting err");
//	return controlr.exec( "options(error=dump.frames)");

	//controlr.internal( `x<- data.frame( A=rep(c("A","B"),3), B=seq(1,6)); x` );
	console.info( "sending cmd" );
	return controlr.internal( `xx <- 2+21; xx` );
	
	
}).then( function(a){
	
	console.info("A1: ", arguments);
	return controlr.internal( `ls()` );
	
}).then( function(a){

	console.info("A2: ", arguments);
	return controlr.internal( `.GlobalEnv` );

}).then( function(a){

	console.info("A3: ", arguments);
	//return controlr.internal( `getCRANmirrors()` );
	//return controlr.internal( `options('repos')` );
	return controlr.internal( `getOption('repos')` );

}).then( function(a){

	console.info("A4: ", arguments);
	return controlr.internal( "list(A=1, B=2, 3, 4, X=14 )");

}).then( function(a){

	console.info("A5: ", arguments);
	return controlr.shutdown();

}).then( function(a){
	console.info( "[end test -- should shutdown cleanly]" );	
});

// repl.start('> ').context.controlr = controlr;


