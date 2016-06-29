{
	"targets": [
		{
			"target_name": "controlr",
			"type": "executable",
			"sources": [ 
				"src/controlr.cc", 
				"src/rinterface_common.cc",
                "src/JSON.cc"
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
			
			'conditions': [
				['OS=="linux"', {
					"cflags": [
						'<!@(R CMD config --cppflags)'
					],
					"sources": [ 
						"src/rinterface_linux.cc"
					],
					"libraries": [
						'-lR', '../lib/libuv.a'
					],
					"library_dirs" : [ 
					],
					"include_dirs" : [
					],
					"ldflags" : [
						'<!@(R CMD config --ldflags)', '<!(echo $LDFLAGS)'
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
						'<!@(Rscript -e "cat(R.home())")/lib',
					],
					"ldflags" : [
						'<!@(R CMD config --ldflags)'
					],
					"xcode_settings": {
						'MACOSX_DEPLOYMENT_TARGET': '10.11',
						'OTHER_CFLAGS': ["-std=c++11", "-stdlib=libc++", "-fexceptions", '<!@(R CMD config --cppflags)']
					}
          	}],
				['OS=="win"', {
					"sources": [ 
						"src/rinterface_win.cc"
					],
					"library_dirs" : [ 
					],
					"include_dirs" : [
						'<!@(Rscript -e "cat(R.home())")/include',
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
