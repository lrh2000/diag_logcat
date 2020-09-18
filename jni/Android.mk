LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := diag_logcat
LOCAL_SRC_FILES := main.c diag_serial.c diag_char.c
LOCAL_LDLIBS := -ldl

include $(BUILD_EXECUTABLE)
