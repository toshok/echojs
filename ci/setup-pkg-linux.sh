set -ex
wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key|sudo apt-key add -
sudo apt-add-repository -y 'deb http://llvm.org/apt/trusty/ llvm-toolchain-trusty-3.8 main'
sudo apt-get update
sudo apt-get install libunwind8-dev make autoconf automake libedit-dev zlib1g-dev cmake
