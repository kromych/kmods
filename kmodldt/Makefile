KDIR ?= /lib/modules/$(shell uname -r)/build
BUILD_DIR ?= $(PWD)/build
BUILD_DIR_MAKEFILE ?= $(PWD)/build/Makefile

default: $(BUILD_DIR_MAKEFILE)
	-rm $(PWD)/../kinclude
	-rm -rf $(PWD)/../kasm
	-mkdir -p $(PWD)/../kasm
	-ln -s $(KDIR)/include $(PWD)/../kinclude
	-ln -s $(KDIR)/include/asm-generic $(PWD)/../kasm/asm
	make -C $(KDIR) M=$(BUILD_DIR) src=$(PWD) modules
	gcc kmodldt-test.c -o $(BUILD_DIR)/kmodldt-test -fPIE -static -ggdb3

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR_MAKEFILE): $(BUILD_DIR)
	touch "$@"

clean:
	#make -C $(KDIR) M=$(BUILD_DIR) src=$(PWD) clean
	-rm -rf $(BUILD_DIR)
