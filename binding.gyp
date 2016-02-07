{
	"targets": [
		
		{
			"target_name": "controlr",
			"type": "executable",
			"sources": [ 
                "src/controlr.cc", 
                "src/rinterface_common.cc", 
                "src/rinterface_linux.cc" 
            ],
            "libraries": [
               '-lR', '-lRblas', '../lib/libuv.a'
            ],
			"libraries!": [
				'-l"<(node_root_dir)/$(ConfigurationName)/<(node_lib_file)"'
			],
			"library_dirs" : [ 
				'<!@(printf "%s/lib" "$R_HOME")',
                "./lib" 
            ],
			"include_dirs" : [
				"./include/",
				'<!@(printf "%s/include" "$R_HOME")',
				'<!@(printf "%s/src/extra/graphapp" "$R_HOME")'
			],
            "cflags!" : [ "-fno-exceptions" ],
            "cflags" : [ "-fexceptions" ],
            "cflags_cc!" : [ "-fno-exceptions" ],
            "cflags_cc" : [ "-fexceptions" ],
            "ldflags" : [
				'<!@($R_HOME/bin/R CMD config --ldflags)'
            ]
		}

	]
}
