set -ex
#sudo apt-get update
#sudo apt-get install software-properties-common python-software-properties
sudo add-apt-repository -y ppa:chris-lea/node.js
sudo apt-add-repository -y 'deb http://us.archive.ubuntu.com/ubuntu/ precise universe'
sudo apt-add-repository -y 'deb http://us.archive.ubuntu.com/ubuntu/ precise-updates universe'
sudo apt-add-repository -y ppa:ubuntu-toolchain-r/test
wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key|sudo apt-key add -
sudo apt-add-repository -y 'deb http://llvm.org/apt/precise/ llvm-toolchain-precise-3.6 main'
sudo apt-get update
sudo apt-get install libunwind7-dev make autoconf automake libedit-dev gcc-4.9 g++-4.9 zlib1g-dev
sudo update-alternatives --install /usr/bin/gcc      gcc /usr/bin/gcc-4.9 10
sudo update-alternatives --install /usr/bin/g++      g++ /usr/bin/g++-4.9 10
