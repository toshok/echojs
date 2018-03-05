set -e

NPM=`which npm`

sudo apt-get install nodejs build-essential
sudo $NPM install -g node-gyp
