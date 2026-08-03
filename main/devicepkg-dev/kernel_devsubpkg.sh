#!/bin/sh

builddir=$1
subpkgdir=$2
_outdir=$3
_carch=$4
_flavor=$5

[ -z "$LLVM" ] || _llvm="LLVM=$LLVM"

if [ -z "$builddir" ] || [ -z "$subpkgdir" ] || [ -z "$_outdir" ] ||
	[ -z "$_carch" ] || [ -z "$_flavor" ]; then
	echo "ERROR: missing argument!"
	echo "Please call kernel_devsubpkg() with \$builddir, \$subpkgdir,"
	echo " \$_outdir, \$_carch, \$_flavor as arguments."
	exit 1
fi

# This is an adaptation of upstream Alpine Linux _dev() on linux-lts
# APKBUILD, which is itself an adaptation of what ubuntu does.
# The goal is to have the minimal set of files to compile out-of-tree modules.
cd "$builddir" || exit 1

_abi_release="$(make -C "$_outdir" -s kernelrelease)"

_kernelheaderdir="$subpkgdir"/usr/src/linux-headers-"$_abi_release"

# From now on, minus variable names substitutions, the rest of this
# function is taken from Alpine APKBUILD for linux-lts available here:
# https://git.alpinelinux.org/aports/tree/main/linux-lts/APKBUILD#n268

# first we import config, run prepare to set up for building
# external modules, and create the scripts
mkdir -p "$_kernelheaderdir"
cp -a "$_outdir"/.config "$_kernelheaderdir"/

install -D -t "$_kernelheaderdir"/certs "$_outdir"/certs/signing_key.x509 || :

make -C "$builddir" \
	O="$_kernelheaderdir" \
	ARCH="$_carch" \
	$_llvm \
	prepare modules_prepare scripts

# remove the stuff that points to real sources. we want 3rd party
# modules to believe this is the sources
rm "$_kernelheaderdir"/Makefile "$_kernelheaderdir"/source

# copy the needed stuff from real sources
#
# this is taken from ubuntu kernel build script
# http://kernel.ubuntu.com/git/ubuntu/ubuntu-zesty.git/tree/debian/rules.d/3-binary-indep.mk

find .  -path './include/*' -prune \
	-o -path './scripts/*' -prune -o -type f \
	\( -name 'Makefile*' -o -name 'Kconfig*' -o -name 'Kbuild*' -o \
	   -name '*.sh' -o -name '*.pl' -o -name '*.lds' -o -name 'Platform' \) \
	-print | cpio -pdm "$_kernelheaderdir"

cp -a scripts include "$_kernelheaderdir"

find "arch/$_carch" "tools/include" "tools/arch/$_carch" -type f -path '*/include/*' \
	-print | cpio -pdm "$_kernelheaderdir"

install -Dm644 "$_outdir"/Module.symvers \
	"$_kernelheaderdir"/Module.symvers

# remove unneeded things
msg "Removing documentation..."
rm -r "$_kernelheaderdir"/Documentation
sed -i -e '/Documentation/d' "$_kernelheaderdir"/Kconfig
find "$_kernelheaderdir" -type f \( -name '*.o' -o -name '*.cmd' \) -exec rm -v -- {} +

mkdir -p "$subpkgdir"/lib/modules/"$_abi_release"
ln -sf /usr/src/linux-headers-"$_abi_release" \
	"$subpkgdir"/lib/modules/"$_abi_release"/build

