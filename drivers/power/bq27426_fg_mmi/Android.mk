DLKM_DIR := motorola/kernel/modules
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := bq27426_fg_mmi.ko
LOCAL_MODULE_TAGS := optional

ifeq ($(ATL_4000MAH_8A_BATTERY_PROFILE),true)
	KERNEL_CFLAGS += CONFIG_ATL_4000MAH_8A_BATTERY_PROFILE=y
endif

ifeq ($(SCUD_4000MAH_8A_BATTERY_PROFILE),true)
	KERNEL_CFLAGS += CONFIG_SCUD_4000MAH_8A_BATTERY_PROFILE=y
endif

ifeq ($(ATL_LS40_1545MAH_BATTERY_PROFILE),true)
	KERNEL_CFLAGS += CONFIG_ATL_LS40_1545MAH_BATTERY_PROFILE=y
endif

ifeq ($(ATL_LS30_1255MAH_BATTERY_PROFILE),true)
	KERNEL_CFLAGS += CONFIG_ATL_LS30_1255MAH_BATTERY_PROFILE=y
endif

ifeq ($(DLKM_INSTALL_TO_VENDOR_OUT),true)
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/lib/modules/
else
LOCAL_MODULE_PATH := $(KERNEL_MODULES_OUT)
endif

include $(DLKM_DIR)/AndroidKernelModule.mk
