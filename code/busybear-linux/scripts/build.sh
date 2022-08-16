#!/bin/bash

set -e

. conf/busybear.config

#
# test environment
#
for var in ARCH ABI CROSS_COMPILE BUSYBOX_VERSION \
    DROPBEAR_VERSION LINUX_KERNEL_VERSION; do
    if [ -z "${!var}" ]; then
        echo "${!var} not set" && exit 1
    fi
done

#
# find executables
#
for prog in ${CROSS_COMPILE}gcc nproc curl openssl rsync; do
    if [ -z $(which ${prog}) ]; then
        echo "error: ${prog} not found in PATH" && exit 1
    fi
done

#
# download busybox, dropbear and linux
#
export MAKEFLAGS=-j32
test -d archives || mkdir archives
test -f archives/busybox-${BUSYBOX_VERSION}.tar.bz2 || \
    curl -L -o archives/busybox-${BUSYBOX_VERSION}.tar.bz2 \
        https://busybox.net/downloads/busybox-${BUSYBOX_VERSION}.tar.bz2
test -f archives/dropbear-${DROPBEAR_VERSION}.tar.bz2 || \
    http --download https://matt.ucc.asn.au/dropbear/releases/dropbear-${DROPBEAR_VERSION}.tar.bz2 -o archives/dropbear-${DROPBEAR_VERSION}.tar.bz2

#
# extract busybox, dropbear and linux
#
test -d build || mkdir build
test -d build/busybox-${BUSYBOX_VERSION} || \
    tar -C build -xjf archives/busybox-${BUSYBOX_VERSION}.tar.bz2
test -d build/dropbear-2022.82 || \
    tar -C build -xjf archives/dropbear-2022.82.tar.bz2
#
# set default configurations
#
cp conf/busybox.config build/busybox-${BUSYBOX_VERSION}/.config

#
# build busybox, dropbear and linux
#
test -x build/busybox-${BUSYBOX_VERSION}/busybox || (
    cd build/busybox-${BUSYBOX_VERSION}
    make ARCH=riscv CROSS_COMPILE=${CROSS_COMPILE} oldconfig
    make ARCH=riscv CROSS_COMPILE=${CROSS_COMPILE} -j$(nproc)
)
test -x build/dropbear-${DROPBEAR_VERSION}/dropbear || (
    cd build/dropbear-${DROPBEAR_VERSION}
    ./configure --host=${CROSS_COMPILE%-} --disable-zlib
    make -j$(nproc)
)

#
# we can use qume / don't need
#
# test -d build/riscv-pk || mkdir build/riscv-pk
# test -x build/riscv-pk/bbl || (
#     cd build/riscv-pk
#     ../../src/riscv-pk/configure \
#         --enable-logo \
#         --host=riscv64-unknown-linux-gnu \
#         --with-payload=../linux-${LINUX_KERNEL_VERSION}/vmlinux
#     make -j$(nproc)
# )
#
# create filesystem image
#
sudo env PATH=${PATH} UID=$(id -u) GID=$(id -g) \
./scripts/image.sh
