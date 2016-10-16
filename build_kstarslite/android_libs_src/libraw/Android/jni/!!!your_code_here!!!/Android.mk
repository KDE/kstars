LOCAL_PATH_ := $(call my-dir)

include $(call all-subdir-makefiles)

include $(CLEAR_VARS)

#change to your name
LOCAL_MODULE    := RAWExtractor

LOCAL_SRC_FILES := $(wildcard $(LOCAL_PATH_)/*.cpp)

LOCAL_STATIC_LIBRARIES := jpeg raw 

include $(BUILD_SHARED_LIBRARY)
