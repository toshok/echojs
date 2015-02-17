
DIR=$(cd $(dirname $0) && pwd)

sudo apt-get install clang-3.4 llvm-3.4 libunwind8-dev libuv-dev
sudo npm install -g node-gyp
sudo npm install -g coffee-script

cd $DIR/../..
make all bootstrap check
