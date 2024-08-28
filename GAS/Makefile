KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

obj-m += gas_detector.o

CFLAGS_USER := -Wall -Wextra -O2
USER_PROG := gas_monitor

.PHONY: all clean user kernel install uninstall setup_gpio cleanup_gpio

all: kernel user

kernel:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

user: $(USER_PROG)

$(USER_PROG): gas_monitor.c
	$(CC) $(CFLAGS_USER) -o $@ $<

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f $(USER_PROG)

install: kernel
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules_install
	sudo insmod gas_detector.ko
	sudo chown root:root $(USER_PROG)
	sudo chmod 755 $(USER_PROG)
	sudo cp $(USER_PROG) /usr/local/bin/

uninstall:
	sudo rmmod gas_detector
	sudo rm -f /usr/local/bin/$(USER_PROG)

setup_gpio:
	@echo "Setting up GPIO..."
	@for pin in 17 27 22 24; do \
		if [ ! -e /sys/class/gpio/gpio$$pin ]; then \
			echo $$pin > /sys/class/gpio/export || echo "Failed to export GPIO $$pin"; \
		fi; \
		echo out > /sys/class/gpio/gpio$$pin/direction || echo "Failed to set direction for GPIO $$pin"; \
	done
	@echo "GPIO setup complete."

cleanup_gpio:
	@echo "Cleaning up GPIO..."
	@for pin in 17 27 22 24; do \
		if [ -e /sys/class/gpio/gpio$$pin ]; then \
			echo $$pin > /sys/class/gpio/unexport || echo "Failed to unexport GPIO $$pin"; \
		fi; \
	done
	@echo "GPIO cleanup complete."
