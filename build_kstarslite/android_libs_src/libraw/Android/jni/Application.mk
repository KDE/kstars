APP_OPTIM := release

APP_LDFLAGS += -fopenmp -Wl,--no-undefined

ADDITIONAL_FLAGS := -Ofast -fopenmp

APP_CFLAGS += $(ADDITIONAL_FLAGS)

APP_CPPFLAGS += $(ADDITIONAL_FLAGS) -std=c++11

APP_LDLIBS += -lz

APP_PLATFORM := android-15 

APP_STL := gnustl_static

APP_ABI := armeabi-v7a 
