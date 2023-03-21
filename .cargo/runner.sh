#! /bin/bash

KERNEL=$1

cp $KERNEL conf/limine.cfg target/limine/limine{.sys,-cd.bin,-cd-efi.bin} target/iso_root

xorriso -as mkisofs                                             \
    -b limine-cd.bin                                            \
    -no-emul-boot -boot-load-size 4 -boot-info-table            \
    --efi-boot limine-cd-efi.bin                                \
    -efi-boot-part --efi-boot-image --protective-msdos-label    \
    target/iso_root -o $KERNEL.iso

target/limine/limine-deploy $KERNEL.iso

qemu-system-x86_64 -D target/log.txt $KERNEL.iso
