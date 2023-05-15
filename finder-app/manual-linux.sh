#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

CURR_DIR=$(pwd)
OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
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

if [ ! -d "${OUTDIR}" ]; then
    echo "Could not create ${OUTDIR}, exiting..."
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
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ];
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
cd "${OUTDIR}"
mkdir -p rootfs/bin rootfs/dev rootfs/etc rootfs/home rootfs/lib rootfs/lib64 rootfs/proc rootfs/sbin rootfs/sys rootfs/tmp rootfs/usr rootfs/var
mkdir -p rootfs/usr/bin rootfs/usr/lib rootfs/usr/sbin
mkdir -p rootfs/var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ];
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
cd "${OUTDIR}"/rootfs

# echo "Library dependencies"
# ${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
# ${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
# Interpreter library section
tmp_output=$(readelf -a ${OUTDIR}/rootfs/bin/busybox | grep -o -P '(?<=interpreter: ).*(?=])')
lib_basename=$(basename -a ${tmp_output})
lib_dirname=$(dirname ${tmp_output})

lib_sysroot_path=$(find $(${CROSS_COMPILE}gcc -print-sysroot) -name ${lib_basename})

echo "cp ${lib_sysroot_path} ${OUTDIR}/rootfs${tmp_output}..."
mkdir -p $(dirname ${OUTDIR}/rootfs${tmp_output})		# This will most likely not be necessary since the folder structure has already been created
cp ${lib_sysroot_path} ${OUTDIR}/rootfs${tmp_output}

# Shared libraries section
readelf -a ${OUTDIR}/rootfs/bin/busybox | grep -o -P '(?<=Shared library: \[).*(?=])' | while read -r lib ; do
    lib_basename=$(basename -a ${lib})
    lib_dirname=$(dirname ${lib})
    lib_sysroot_path=$(find $(${CROSS_COMPILE}gcc -print-sysroot) -name ${lib_basename})
    # todo: instead of hardcoding lib64, extract it from find's output
    echo "cp ${lib_sysroot_path} ${OUTDIR}/rootfs/lib64/${lib}"
    mkdir -p $(dirname ${OUTDIR}/rootfs/${lib})
    cp ${lib_sysroot_path} ${OUTDIR}/rootfs/lib64/${lib}
done

# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
cd ${CURR_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp autorun-qemu.sh ${OUTDIR}/rootfs/home
cp finder-test.sh ${OUTDIR}/rootfs/home
cp writer.sh ${OUTDIR}/rootfs/home
cp writer ${OUTDIR}/rootfs/home
cp finder.sh ${OUTDIR}/rootfs/home
mkdir -p ${OUTDIR}/rootfs/home/conf
cp conf/username.txt ${OUTDIR}/rootfs/home/conf
cp conf/assignment.txt ${OUTDIR}/rootfs/home/conf

# TODO: Chown the root directory
sudo chown root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip -f ${OUTDIR}/initramfs.cpio
