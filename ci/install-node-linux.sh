set -ex

NPM=`which npm`

sudo apt-get install nodejs build-essential
sudo $NPM install -g node-gyp
