include ../../../tools/top.mk
include ../../make.mk
#include lib/libscpi/Makefile

INC += \
	src \
	$(TOP)/hw \
	$(TOP)/lib/libscpi/inc \
	$(TOP)/lib/libscpi/src

# Example source
EXAMPLE_SOURCE += $(wildcard src/*.c)
SRC_C += $(addprefix $(CURRENT_PATH)/, $(EXAMPLE_SOURCE))

include ../../rules.mk
