#!/bin/sh

apt-get update
apt-get install -y llvm-3.4 clang-3.4 make git nodejs-legacy node-gyp npm libunwind8-dev libuv-dev build-essential dh-make bzr-builddeb
npm install -g coffee-script
npm install -g mocha
