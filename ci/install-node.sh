# travis-ci osx nodes are mountain_lion, not mavericks
#OSX=mavericks
OSX=mountain_lion
/usr/bin/curl '-f#LA Homebrew 0.9.5 (Ruby 1.8.7-358; Mac OS X 10.8.5)' https://downloads.sf.net/project/machomebrew/Bottles/node-0.10.25.$OSX.bottle.tar.gz -C 0 -o node-0.10.25.$OSX.bottle.tar.gz
tar -C /usr/local/Cellar -zxf node-0.10.25.$OSX.bottle.tar.gz
rm node-0.10.25.$OSX.bottle.tar.gz
brew link node
npm install -g node-gyp
