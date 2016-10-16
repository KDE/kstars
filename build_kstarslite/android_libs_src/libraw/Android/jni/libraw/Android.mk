LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libraw

#path to LibRAW source root:
LIBRAW_PATH := $(LOCAL_PATH)/../../..

LOCAL_CFLAGS := -DUSE_LCMS2 -DUSE_JPEG -w

LOCAL_CPPFLAGS := $(LOCAL_CFLAGS) -fexceptions

LOCAL_C_INCLUDES := $(LIBRAW_PATH)/internal $(LIBRAW_PATH)/libraw $(LIBRAW_PATH)/

FILE_LIST := $(LOCAL_PATH)/swab.c \
$(LIBRAW_PATH)/internal/dcraw_common.cpp \
$(LIBRAW_PATH)/internal/dcraw_fileio.cpp \
$(LIBRAW_PATH)/internal/demosaic_packs.cpp \
$(LIBRAW_PATH)/src/libraw_datastream.cpp \
$(LIBRAW_PATH)/src/libraw_cxx.cpp	
							
LOCAL_EXPORT_C_INCLUDES	:= $(LIBRAW_PATH)/libraw

LOCAL_SRC_FILES := $(FILE_LIST)

LOCAL_STATIC_LIBRARIES := lcms2 jpeg

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
