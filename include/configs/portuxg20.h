/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * taskit PortuxG20 (Stamp9G20 module, AT91SAM9G20, 64 MiB SDRAM, 128 MiB NAND)
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <asm/hardware.h>
#include <linux/sizes.h>

#define CFG_SYS_AT91_SLOW_CLOCK		32768
#define CFG_SYS_AT91_MAIN_CLOCK		18432000

#define CFG_USART_BASE			ATMEL_BASE_DBGU
#define CFG_USART_ID			ATMEL_ID_SYS

#define CFG_SYS_SDRAM_BASE		ATMEL_BASE_CS1
#define CFG_SYS_SDRAM_SIZE		(64 * SZ_1M)

#define CFG_SYS_INIT_RAM_SIZE		0x1000
#define CFG_SYS_INIT_RAM_ADDR		ATMEL_BASE_SRAM1

/* NAND: NCS3, ALE = A21, CLE = A22, nCE = PC14, R/B = PC13 */
#define CFG_SYS_NAND_BASE		ATMEL_BASE_CS3
#define CFG_SYS_NAND_MASK_ALE		(1 << 21)
#define CFG_SYS_NAND_MASK_CLE		(1 << 22)
#define CFG_SYS_NAND_ENABLE_PIN		AT91_PIN_PC14
#define CFG_SYS_NAND_READY_PIN		AT91_PIN_PC13

/* USB host (OHCI, legacy non-DM driver) */
#define CFG_SYS_USB_OHCI_REGS_BASE	ATMEL_UHP_BASE

/* SPL: load U-Boot proper (raw) from NAND 0x20000, skipping bad blocks */
#define CFG_SYS_NAND_U_BOOT_SIZE	SZ_512K
#define CFG_SYS_NAND_U_BOOT_START	CONFIG_TEXT_BASE
#define CFG_SYS_NAND_U_BOOT_DST		CONFIG_TEXT_BASE

/* 1-bit software Hamming, the layout Linux uses for nand-ecc-mode = "soft" */
#define CFG_SYS_NAND_ECCSIZE		256
#define CFG_SYS_NAND_ECCBYTES		3
#define CFG_SYS_NAND_ECCPOS		{ 40, 41, 42, 43, 44, 45, 46, 47, \
					  48, 49, 50, 51, 52, 53, 54, 55, \
					  56, 57, 58, 59, 60, 61, 62, 63, }

/* PLLA 792 MHz, PCK 396 MHz, MCK 132 MHz from 18.432 MHz */
#define CFG_SYS_MASTER_CLOCK		132096000
#define AT91_PLL_LOCK_TIMEOUT		1000000
#define CFG_SYS_AT91_PLLA		0x202A3F01
#define CFG_SYS_MCKR			0x1300
#define CFG_SYS_MCKR_CSS		(0x02 | CFG_SYS_MCKR)
#define CFG_SYS_AT91_PLLB		0x10193F05

/*
 * NAND layout (128 KiB blocks). Each partition after the SPL has spare
 * blocks: the SPL and "nand read" skip bad blocks, so the boot commands
 * read a fixed size short of the partition end.
 *   0x0000000  128k  spl         boot.bin, read by the ROM (no ECC)
 *   0x0020000    1M  uboot       U-Boot proper (the SPL reads 512 KiB), or barebox
 *   0x0120000  256k  env         CONFIG_ENV_RANGE
 *   0x0160000  256k  env_redund
 *   0x01a0000  384k  dtb
 *   0x0200000    8M  kernel      zImage
 *   0x0a00000   16M  rescue      rescue initramfs, rootfs.cpio.uboot
 *   0x1a00000     -  ubi
 */
#define PORTUXG20_MTDPARTS \
	"atmel_nand:128k(spl)ro,1M(uboot)ro,256k(env),256k(env_redund)," \
	"384k(dtb),8M(kernel),16M(rescue),-(ubi)"

/*
 * RAM: the kernel decompresses itself to 0x20008000, and U-Boot keeps the
 * 16 MiB below its stack (from about 0x22b00000) for itself.
 *   0x21000000  zImage, up to 8 MiB
 *   0x21800000  device tree
 *   0x21a00000  initramfs, up to 17 MiB
 *
 * Boot commands: nfsboot (zImage and device tree over TFTP, root read-only
 * over NFS from ${nfsroot}, the board's writable ${nfsstate} as
 * boardstate=; the kernel configures eth0 by DHCP itself, which also gives
 * it the name server and domain), nandboot (rescue system from NAND),
 * tftpboot_initrd (rescue system over TFTP). ${tftpserver} overrides the
 * DHCP server address, ${hostname} becomes the kernel's hostname=,
 * ${extraargs} is appended.
 */
#define CFG_EXTRA_ENV_SETTINGS \
	"mtdparts=mtdparts=" PORTUXG20_MTDPARTS "\0" \
	"kernel_addr_r=0x21000000\0" \
	"fdt_addr_r=0x21800000\0" \
	"ramdisk_addr_r=0x21a00000\0" \
	"kernel_size=0x700000\0" \
	"fdt_size=0x20000\0" \
	"rescue_size=0xe00000\0" \
	"tftpdir=portuxg20\0" \
	"fdtfile=at91-portuxg20.dtb\0" \
	"nfsroot=/srv/nfs/portuxg20\0" \
	"console=ttyS0,115200\0" \
	"setargs=setenv bootargs console=${console} ${mtdparts} ${rootargs} " \
		"${extraargs}; test -z \"${hostname}\" || " \
		"setenv bootargs ${bootargs} hostname=${hostname}\0" \
	"netup=setenv autoload no; dhcp && run tftpsrv\0" \
	"tftpsrv=test -z \"${tftpserver}\" || setenv serverip ${tftpserver}\0" \
	"nfsargs=setenv rootargs root=/dev/nfs ro " \
		"nfsroot=${serverip}:${nfsroot},v3,tcp,rsize=32768,wsize=32768 " \
		"ip=::::${hostname}:eth0:dhcp; " \
		"test -z \"${nfsstate}\" || " \
		"setenv rootargs ${rootargs} boardstate=${serverip}:${nfsstate}\0" \
	"nfsboot=run netup && " \
		"tftpboot ${kernel_addr_r} ${tftpdir}/zImage && " \
		"tftpboot ${fdt_addr_r} ${tftpdir}/${fdtfile} && " \
		"run nfsargs setargs && bootz ${kernel_addr_r} - ${fdt_addr_r}\0" \
	"nandboot=nand read ${kernel_addr_r} kernel ${kernel_size} && " \
		"nand read ${fdt_addr_r} dtb ${fdt_size} && " \
		"nand read ${ramdisk_addr_r} rescue ${rescue_size} && " \
		"setenv rootargs && run setargs && " \
		"bootz ${kernel_addr_r} ${ramdisk_addr_r} ${fdt_addr_r}\0" \
	"tftpboot_initrd=run netup && " \
		"tftpboot ${kernel_addr_r} ${tftpdir}/zImage && " \
		"tftpboot ${fdt_addr_r} ${tftpdir}/${fdtfile} && " \
		"tftpboot ${ramdisk_addr_r} ${tftpdir}/rootfs.cpio.uboot && " \
		"setenv rootargs && run setargs && " \
		"bootz ${kernel_addr_r} ${ramdisk_addr_r} ${fdt_addr_r}\0"

#endif /* __CONFIG_H */
