KDIR ?= /lib/modules/$(shell uname -r)/build
BUILD_DIR ?= $(PWD)/build
BUILD_DIR_MAKEFILE ?= $(PWD)/build/Makefile

default: $(BUILD_DIR_MAKEFILE)
	-rm $(PWD)/../kinclude
	-ln -rs $(KDIR)/include $(PWD)/../kinclude
	make -C $(KDIR) M=$(BUILD_DIR) src=$(PWD) modules
	gcc kmodprocfs-test.c -o $(BUILD_DIR)/kmodprocfs-test

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR_MAKEFILE): $(BUILD_DIR)
	touch "$@"

clean:
	make -C $(KDIR) M=$(BUILD_DIR) src=$(PWD) clean
	-rm kmodprocfs-test
