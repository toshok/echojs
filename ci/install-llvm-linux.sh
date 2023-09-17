set -e
sudo apt-get install llvm-15-dev clang-15
sudo update-alternatives --install /usr/bin/llc      llc /usr/bin/llc-15 10 --force
sudo update-alternatives --install /usr/bin/opt      opt /usr/bin/opt-15 10 --force
sudo update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-15 10 --force
sudo update-alternatives --install /usr/bin/llvm-dis llvm-dis /usr/bin/llvm-dis-15 10 --force
sudo update-alternatives --install /usr/bin/llvm-as  llvm-as  /usr/bin/llvm-as-15 10 --force
sudo update-alternatives --install /usr/bin/clang    clang    /usr/bin/clang-15 10 --force
sudo update-alternatives --install /usr/bin/clang++  clang++  /usr/bin/clang++-15 10 --force
llvm-config --version
clang++ --version
clang --version
export LLVM_SUFFIX=
