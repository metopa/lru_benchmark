#!/usr/bin/env bash

sudo apt-get install \
    git \
    g++ \
    cmake \
    libboost-all-dev \
    libevent-dev \
    libdouble-conversion-dev \
    libgoogle-glog-dev \
    libgflags-dev \
    libiberty-dev \
    liblz4-dev \
    liblzma-dev \
    libsnappy-dev \
    make \
    zlib1g-dev \
    binutils-dev \
    libjemalloc-dev \
    libssl-dev \
    git-lfs \
    pkg-config

PROJECT_DIR="$(pwd)"

cd ~
git clone https://github.com/facebook/folly.git
cd folly
mkdir _build && cd _build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j $(nproc)
sudo make install

cd "$PROJECT_DIR"

sudo apt-get install python3-pip
sudo pip3 install virtualenv
virtualenv -p /usr/bin/python3 venv

git lfs install
git lfs fetch

mkdir cmake-build-release
cd cmake-build-release
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j $(nproc)

echo Done
