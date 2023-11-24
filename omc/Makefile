# Makefile

TOP_DIR = $(shell pwd)/..

include $(TOP_DIR)/Rules.mak
LIBNAME=$(TOP_DIR)/lib/omc.a

all : $(LIBNAME)

EXTRA_CFLAGS := -I../ds_public -I../snmp -I./common -I./db -I./cfg -I./sys -I./APP -I./ATS -I./SA -I./agent -I./omt
EXTRA_LDFLAGS :=

CSRCS := ../snmp/snmp_nms.c \
		../gos/zip/quicklz/gos_quicklz.c \
		../gos/zip/quicklz/quicklz.c

CPPSRCS := \
	common/omc_public.cpp\
	db/OmcDao.cpp\
	db/TaskDBSync.cpp\
	omt/TaskOmt.cpp\
	agent/TaskQuery.cpp\
	agent/TaskSnmp.cpp\
	sys/TaskSys.cpp\
	sys/TaskDataSync.cpp\
	agent/TaskTXAgent.cpp\
	agent/TrapMsg.cpp
	
clean-extra :
	- $(RM) $(LIBNAME)
	- $(RM) */*.o

include $(TOP_DIR)/Makerules
