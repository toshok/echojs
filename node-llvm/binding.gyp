{
    "targets": [{
        "target_name": "llvm",
	"cxxflags": ['<!@(echo $LLVM_CXXFLAGS)'],
	"include_dirs": ['<!@(echo $LLVM_INCLUDEDIR)'],
	"defines": ['<!@(echo $LLVM_DEFINES)'],
        "sources": ['<!@(ls -1 *.cpp)'],
	"link_settings": {
	    "libraries": ['<!@(echo $LLVM_LINKFLAGS)']
	},
	"xcode_settings": {
	        'MACOSX_DEPLOYMENT_TARGET': "<!(echo $MIN_OSX_VERSION)"
	}
    }]
} 
