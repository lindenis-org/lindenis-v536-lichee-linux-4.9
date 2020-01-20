obj-$(CONFIG_SUNXI_NAND) += nand.o

ifdef CONFIG_ARM64
libnand = libnand64.a
endif

ifdef CONFIG_ARM
libnand = libnand32.a
endif

$(obj)/$(libnand) : FORCE

ifneq ($(wildcard $(obj)/lib),)
	$(MAKE) -C $(obj)/..\
		LICHEE_KDIR="$(shell pwd)" \
		build
endif
	cp $(obj)/$(basename $(libnand)) $@

nand-objs += nand_interface.o \
	nfd/nand_ota_burn.o \
	nfd/nand_base.o \
	nfd/nand_blk.o \
	nfd/nand_dev.o \
	nfd/nand_class.o \
	nfd/nand_test.o \
	nfd/nand_osal_for_linux.o \
	nfd/nand_lib_cfg.o \
	$(libnand)
