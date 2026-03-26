#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

CURDIR="$PWD"
OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}
status=$?

# Give an error if the create folder was not successful
if [ $status -ne 0 ]
then
    echo "Error: Failed creating ${OUTDIR} folder with status $status"
    exit 1
fi

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    echo "Starting Kernel build ... "
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
    echo "Kernel build completed."
fi

echo "Adding the Image in outdir"
cp "${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image" "$OUTDIR/Image"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p "${OUTDIR}/rootfs"
cd "$OUTDIR/rootfs"
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log
mkdir -p home/conf
chmod 1777 ${OUTDIR}/rootfs/tmp

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    echo "Configure Busybox ..."
    make distclean
    make defconfig
    sed -i 's/CONFIG_STATIC=y/# CONFIG_STATIC is not set/' .config
else
    cd busybox
fi

# TODO: Make and install busybox
echo "Make and install Busybox ..."
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
echo "Busybox make and install completed."

echo "Library dependencies"
${CROSS_COMPILE}readelf -a "${OUTDIR}/rootfs/bin/busybox" | grep "program interpreter"
${CROSS_COMPILE}readelf -a "${OUTDIR}/rootfs/bin/busybox" | grep "Shared library"

# TODO: Add library dependencies to rootfs
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
cp -L "${SYSROOT}/lib/ld-linux-aarch64.so.1" "${OUTDIR}/rootfs/lib/"
cp -L "${SYSROOT}/lib64/libc.so.6" "${OUTDIR}/rootfs/lib64/"
cp -L "${SYSROOT}/lib64/libm.so.6" "${OUTDIR}/rootfs/lib64/"
cp -L "${SYSROOT}/lib64/libresolv.so.2" "${OUTDIR}/rootfs/lib64/"

# TODO: Make device nodes
cd "${OUTDIR}/rootfs"
# Create null device 
sudo mknod -m 666 dev/null c 1 3
# Create console device
sudo mknod -m 600 dev/console c 5 1

# TODO: Clean and build the writer utility
cd "$CURDIR"
make clean
make CROSS_COMPILE=aarch64-none-linux-gnu-

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp "${CURDIR}/writer" "${OUTDIR}/rootfs/home/writer"
cp "${CURDIR}/finder.sh" "${OUTDIR}/rootfs/home/finder.sh"
cp "${CURDIR}/conf/username.txt" "${OUTDIR}/rootfs/home/conf/username.txt"
cp "${CURDIR}/conf/assignment.txt" "${OUTDIR}/rootfs/home/conf/assignment.txt"
cp "${CURDIR}/finder-test.sh" "${OUTDIR}/rootfs/home/finder-test.sh"
cp "${CURDIR}/autorun-qemu.sh" "${OUTDIR}/rootfs/home/autorun-qemu.sh"

# TODO: Chown the root directory
sudo chown -R root:root "${OUTDIR}/rootfs"

# TODO: Create initramfs.cpio.gz
cd "${OUTDIR}/rootfs"
find . | cpio -H newc -ov --owner root:root > "${OUTDIR}/initramfs.cpio"
gzip -f "${OUTDIR}/initramfs.cpio"
