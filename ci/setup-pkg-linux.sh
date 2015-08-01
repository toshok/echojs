sudo apt-get-repository ppa:ubuntu-toolchain-r/test
wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key|sudo apt-key add -
sudo apt-add-repository 'deb http://llvm.org/apt/precise/ llvm-toolchain-precise-3.6 main'
sudo apt-get update
sudo apt-get install libuv-dev libunwind8-dev make autoconf automake autoheader libedit-dev clang-3.6 llvm-3.6-dev
