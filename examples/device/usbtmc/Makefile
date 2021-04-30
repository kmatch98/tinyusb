include ../../../tools/top.mk
include ../../make.mk
#include lib/libscpi/Makefile

INC += \
	src \
	$(TOP)/hw \
	$(TOP)/lib/libscpi/inc \
	$(TOP)/lib/libscpi/src \
	$(TOP)/src/mcu/microchip/samd

# Example source
# EXAMPLE_SOURCE += $(wildcard src/*.c) $(wildcard src/mcu/$(VENDOR)/$(CHIP_FAMILY)/*.c)

# SRC_C += $(addprefix $(CURRENT_PATH)/src/, $(EXAMPLE_SOURCE))

$(info SRC_C Makefile: $(SRC_C) )

include ../../rules.mk
