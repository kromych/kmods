KDIR ?= /lib/modules/$(shell uname -r)/build
BUILD_DIR ?= $(PWD)/build
BUILD_DIR_MAKEFILE ?= $(PWD)/build/Makefile

default: $(BUILD_DIR_MAKEFILE)
	-rm $(PWD)/../kinclude
	-rm -rf $(PWD)/../kasm
	-mkdir -p $(PWD)/../kasm
	-ln -rs $(KDIR)/include $(PWD)/../kinclude
	-ln -rs $(KDIR)/include/asm-generic $(PWD)/../kasm/asm
	make -C $(KDIR) M=$(BUILD_DIR) src=$(PWD) modules
	gcc kmodchardev-test.c -o $(BUILD_DIR)/kmodchardev-test

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR_MAKEFILE): $(BUILD_DIR)
	touch "$@"

clean:
	#make -C $(KDIR) M=$(BUILD_DIR) src=$(PWD) clean
	-rm -rf $(BUILD_DIR)
