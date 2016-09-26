set -ex
sudo apt-get install llvm-3.8-dev clang-3.8
sudo update-alternatives --install /usr/bin/llc      llc /usr/bin/llc-3.8 10
sudo update-alternatives --install /usr/bin/opt      opt /usr/bin/opt-3.8 10
sudo update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-3.8 10
sudo update-alternatives --install /usr/bin/llvm-dis llvm-dis /usr/bin/llvm-dis-3.8 10
sudo update-alternatives --install /usr/bin/llvm-as  llvm-as  /usr/bin/llvm-as-3.8 10
sudo update-alternatives --install /usr/bin/clang    clang    /usr/bin/clang-3.8 10
sudo update-alternatives --install /usr/bin/clang++  clang++  /usr/bin/clang++-3.8 10
llvm-config --version
update-alternatives --list llvm-config
type -all llvm-config
