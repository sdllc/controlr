const ControlR = require( "./js/controlr.js")
const repl = require('repl');

var controlr = new ControlR();
controlr.on('console', function(msg){
    console.info(msg);
})

controlr.init({
    rhome: "/home/duncan/dev/R-3.2.3",
    debug: true
});

repl.start('> ').context.controlr = controlr;


