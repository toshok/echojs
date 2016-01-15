TOP=../..

MODULES= --moduledir $(TOP)/modules/objc_internal --moduledir $(TOP)/node-compat 
IMPORT_VARS= -I pirouette=$(shell pwd)/$(TOP)/pirouette/bindings

J3D_DEMOS=	j3d/HelloCube.js \
		j3d/HelloCubemap.js \
		j3d/HelloHead.js \
		j3d/HelloLights.js \
		j3d/HelloPlasma.js \
		j3d/HelloScene.js

J3D_SOURCES= j3d/lib/glMatrix.js \
	    j3d/src/J3D.js \
	    j3d/src/util/Color.js \
	    j3d/src/math/Vector2.js \
	    j3d/src/math/Vector3.js \
	    j3d/src/math/Matrix44.js \
	    j3d/src/engine/Engine.js \
	    j3d/src/engine/Camera.js \
	    j3d/src/engine/Light.js \
	    j3d/src/engine/Geometry.js \
	    j3d/src/engine/Mesh.js \
	    j3d/src/engine/Scene.js \
	    j3d/src/engine/Loader.js \
	    j3d/src/engine/ShaderAtlas.js \
	    j3d/src/engine/Texture.js \
	    j3d/src/engine/Cubemap.js \
	    j3d/src/engine/Transform.js \
	    j3d/src/engine/Postprocess.js \
	    j3d/src/engine/Primitives.js \
	    j3d/src/engine/FrameBuffer.js \
	    j3d/src/engine/ShaderSource.js \
	    j3d/src/engine/Shader.js \
	    j3d/src/util/Time.js \
	    j3d/src/util/ShaderUtil.js \
	    j3d/src/util/Logger.js \
	    j3d/src/engine/BuiltinShaders.js

all: sim dev

SIM_DESTDIR=/Users/toshok/Library/Developer/CoreSimulator/Devices/D8AD0294-5589-48A4-A7F1-311B8A465E69/data/Containers/Bundle/Application/5A803BE1-CF8B-47DE-AEA6-E8A3652EBF2A/test.app
sim: j3d/j3d-all.js
	../../ejs --srcdir $(MODULES) $(IMPORT_VARS) --leave-temp --target sim helloiostableview.js
	cp helloiostableview.js.exe $(SIM_DESTDIR)/test
	mkdir -p $(SIM_DESTDIR)/demo
	cp -r j3d/demo/* $(SIM_DESTDIR)/demo

dev: j3d/j3d-all.js
	../../ejs --srcdir $(MODULES) $(IMPORT_VARS) --leave-temp --target dev helloiostableview.js
	cp helloiostableview.js.exe ipa-work/Payload/HelloIOS.app
	cp -r j3d/demo ipa-work/Payload/HelloIOS.app
	xcrun codesign -s SOMETHING_HERE ipa-work/Payload/HelloIOS.app/helloiostableview.js.exe
	cp ipa-work/Payload/HelloIOS.app/helloiostableview.js.exe "/Users/toshok/Library/Developer/Xcode/iOS DeviceSupport/7.0.4 (11B554a)/Symbols/var/mobile/Applications/EBA2A179-EDF0-4BA7-A097-CAFF316F34C8/HelloIOS.app/"
	xcrun dsymutil ipa-work/Payload/HelloIOS.app/helloiostableview.js.exe -o ipa-work/Payload/HelloIOS.app.dSYM

j3d/j3d-all.js:  $(J3D_SOURCES) j3d/j3d-imports.js
	cat j3d/j3d-imports.js $(J3D_SOURCES) > $@
