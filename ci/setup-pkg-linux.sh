set -ex
wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key|sudo apt-key add -
sudo apt-add-repository -y 'deb http://llvm.org/apt/trusty/ llvm-toolchain-trusty-3.8 main'
sudo apt-get update
sudo apt-get install llvm-3.8-dev clang-3.8 clang++-3.8
sudo apt-get install libunwind8-dev make autoconf automake libedit-dev zlib1g-dev cmake
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-3.8 10
sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-3.8 10
#sudo update-alternatives --install /usr/bin/gcc      gcc /usr/bin/gcc-4.9 10
#sudo update-alternatives --install /usr/bin/g++      g++ /usr/bin/g++-4.9 10

node --version
clang --version
llvm-config --version
