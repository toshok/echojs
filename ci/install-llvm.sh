#brew install -v --disable-shared --without-python --without-libffi llvm34
#OSX=mavericks
#/usr/bin/curl '-f#LA Homebrew 0.9.5 (Ruby 1.8.7-358; Mac OS X 10.8.5)' https://downloads.sf.net/project/machomebrew/Bottles/llvm-3.5.1.$OSX.bottle.tar.gz -C 0 -o llvm-3.5.1.$OSX.bottle.tar.gz
#tar -C /usr/local/Cellar -zxf llvm-3.5.1.$OSX.bottle.tar.gz
#brew switch llvm 3.5.1
#ls -lR /usr/local/Cellar/llvm/3.5.1/include
brew install llvm
brew install clang-format
