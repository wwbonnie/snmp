# Makefile

TOP_DIR = $(shell pwd)/..

include $(TOP_DIR)/Rules.mak

LIBNAME=$(TOP_DIR)/lib/snmp.a

export TOP_DIR

all : $(LIBNAME)

EXTRA_CFLAGS := -I. -I..\gos\public
EXTRA_LDFLAGS :=

CSRC := \
	snmp_nms.c 

CPPSRCS := 

clean-extra :
	- $(RM) $(LIBNAME)
	- $(RM) */*.o

include $(TOP_DIR)/Makerules
