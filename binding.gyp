{
	"targets": [
		{
			"target_name": "controlr",
			"type": "executable",
			"sources": [ 
				"src/controlr.cc", 
				"src/rinterface_common.cc"
			],
			"libraries!": [
				'-l"<(node_root_dir)/$(ConfigurationName)/<(node_lib_file)"'
			],
			"library_dirs" : [ 
				"./lib" 
			],
			"include_dirs" : [
				"./include/"
			],
			"cflags!" : [ "-fno-exceptions" ],
			"cflags" : [ "-fexceptions" ],
			"cflags_cc!" : [ "-fno-exceptions" ],
			"cflags_cc" : [ "-fexceptions" ],
			
			'conditions': [
				['OS=="linux"', {
					"cflags": [
						'<!@($R_HOME/bin/R CMD config --cppflags)'
					],
					"sources": [ 
						"src/rinterface_linux.cc"
					],
					"libraries": [
						'-lR', '../lib/libuv.a'
					],
					"library_dirs" : [ 
						'<!@(printf "%s/lib" "$R_HOME")',
					],
					"include_dirs" : [
						'<!@(printf "%s/include" "$R_HOME")',
						'<!@(printf "%s/src/extra/graphapp" "$R_HOME")'
					],
					"ldflags" : [
						'<!@($R_HOME/bin/R CMD config --ldflags)'
					]
          	}],
				['OS=="mac"', {
					"sources": [ 
						"src/rinterface_linux.cc"
					],
					"libraries": [
						'-lR', '-lRblas', '../lib/osx/libuv.a'
					],
					"library_dirs!" : [ 
						"./lib" 
					],
					"library_dirs" : [ 
						'<!@(printf "%s/lib" "$R_HOME")',
					],
					"include_dirs" : [
						'<!@(printf "%s/include" "$R_HOME")',
						'<!@(printf "%s/src/extra/graphapp" "$R_HOME")'
					],
					"ldflags" : [
						'<!@($R_HOME/bin/R CMD config --ldflags)'
					],
					"xcode_settings": {
						'MACOSX_DEPLOYMENT_TARGET': '10.11',
						'OTHER_CFLAGS': ["-std=c++11", "-stdlib=libc++", "-fexceptions"]
					}
          	}],
				['OS=="win"', {
					"sources": [ 
						"src/rinterface_win.cc"
					],
					"library_dirs" : [ 
					],
					"include_dirs" : [
						'<!@(echo "%R_HOME%/include")',
					],
					"copies": [{
						"destination": "build/Release",
						"files": [ "bin/libuv.dll" ]
					}],
					"libraries": [ 
						"R64.lib", "RGraphApp64.lib", "-llibuv" 
					],
				}]
			]
		}
	]
}
