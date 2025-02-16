#!/bin/bash

# User-controllable options
grub_modinfo_target_cpu=arm
grub_modinfo_platform=efi
grub_disk_cache_stats=0
grub_boot_time_stats=0
grub_have_font_source=1

# Autodetected config
grub_have_asm_uscore=0
grub_bss_start_symbol=""
grub_end_symbol=""

# Build environment
grub_target_cc='arm-linux-gnueabi-gcc'
grub_target_cc_version='arm-linux-gnueabi-gcc (Ubuntu 9.3.0-17ubuntu1~20.04) 9.3.0'
grub_target_cflags='-std=gnu99 -Os -Wall -W -Wshadow -Wpointer-arith -Wundef -Wchar-subscripts -Wcomment -Wdeprecated-declarations -Wdisabled-optimization -Wdiv-by-zero -Wfloat-equal -Wformat-extra-args -Wformat-security -Wformat-y2k -Wimplicit -Wimplicit-function-declaration -Wimplicit-int -Wmain -Wmissing-braces -Wmissing-format-attribute -Wmultichar -Wparentheses -Wreturn-type -Wsequence-point -Wshadow -Wsign-compare -Wswitch -Wtrigraphs -Wunknown-pragmas -Wunused -Wunused-function -Wunused-label -Wunused-parameter -Wunused-value  -Wunused-variable -Wwrite-strings -Wnested-externs -Wstrict-prototypes -g -Wredundant-decls -Wmissing-prototypes -Wmissing-declarations -Wcast-align  -Wextra -Wattributes -Wendif-labels -Winit-self -Wint-to-pointer-cast -Winvalid-pch -Wmissing-field-initializers -Wnonnull -Woverflow -Wvla -Wpointer-to-int-cast -Wstrict-aliasing -Wvariadic-macros -Wvolatile-register-var -Wpointer-sign -Wmissing-include-dirs -Wmissing-prototypes -Wmissing-declarations -Wformat=2 -freg-struct-return -msoft-float -fno-dwarf2-cfi-asm -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-ident -mthumb-interwork -fno-stack-protector -mno-unaligned-access -Wtrampolines -Werror'
grub_target_cppflags=' -Wall -W  -DGRUB_MACHINE_EFI=1 -DGRUB_MACHINE=ARM_EFI -mword-relocations -nostdinc -isystem /usr/lib/gcc-cross/arm-linux-gnueabi/9/include -I$(top_srcdir)/include -I$(top_builddir)/include'
grub_target_ccasflags=' -g  -msoft-float'
grub_target_ldflags=' -Wl,--build-id=none'
grub_cflags=''
grub_cppflags=' -D_FILE_OFFSET_BITS=64'
grub_ccasflags=''
grub_ldflags=''
grub_target_strip='arm-linux-gnueabi-strip'
grub_target_nm='arm-linux-gnueabi-nm'
grub_target_ranlib='arm-linux-gnueabi-ranlib'
grub_target_objconf=''
grub_target_obj2elf=''
grub_target_img_base_ldopt='-Wl,-Ttext'
grub_target_img_ldflags='@TARGET_IMG_BASE_LDFLAGS@'

# Version
grub_version="2.05"
grub_package="grub"
grub_package_string="GRUB 2.05"
grub_package_version="2.05"
grub_package_name="GRUB"
grub_package_bugreport="bug-grub@gnu.org"
