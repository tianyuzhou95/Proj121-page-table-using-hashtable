# SPDX-License-Identifier: GPL-2.0
KCSAN_SANITIZE := n
KCOV_INSTRUMENT := n
UBSAN_SANITIZE := n

CFLAGS_REMOVE_core.o = $(CC_FLAGS_FTRACE)
CFLAGS_REMOVE_debugfs.o = $(CC_FLAGS_FTRACE)
CFLAGS_REMOVE_report.o = $(CC_FLAGS_FTRACE)

CFLAGS_core.o := $(call cc-option,-fno-conserve-stack) \
	$(call cc-option,-mno-outline-atomics) \
	-fno-stack-protector -DDISABLE_BRANCH_PROFILING

obj-y := core.o debugfs.o report.o

KCSAN_INSTRUMENT_BARRIERS_selftest.o := y
obj-$(CONFIG_KCSAN_SELFTEST) += selftest.o

CFLAGS_kcsan_test.o := $(CFLAGS_KCSAN) -g -fno-omit-frame-pointer
obj-$(CONFIG_KCSAN_KUNIT_TEST) += kcsan_test.o
