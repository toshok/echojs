set -ex
#OSX=mavericks
##OSX=mountain_lion
#/usr/bin/curl '-f#LA Homebrew 0.9.5 (Ruby 1.8.7-358; Mac OS X 10.8.5)' https://downloads.sf.net/project/machomebrew/Bottles/node-0.10.25.$OSX.bottle.tar.gz -C 0 -o node-0.10.25.$OSX.bottle.tar.gz
#tar -C /usr/local/Cellar -zxf node-0.10.25.$OSX.bottle.tar.gz
#rm node-0.10.25.$OSX.bottle.tar.gz
#brew unlink node
#brew switch node 0.10.25
npm install -g node-gyp
npm install -g babel@5.5.8
