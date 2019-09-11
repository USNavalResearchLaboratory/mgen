LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := mgen
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../../../protolib/include \
	$(LOCAL_PATH)/../../../include
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)
LOCAL_STATIC_LIBRARIES := protolib

ifeq ($(APP_OPTIM),debug)
	LOCAL_CFLAGS += -DMGEN_DEBUG
endif
LOCAL_EXPORT_CFLAGS := $(LOCAL_CFLAGS)

LOCAL_SRC_FILES := \
	../../../src/common/mgen.cpp \
	../../../src/common/mgenEvent.cpp \
	../../../src/common/mgenFlow.cpp \
	../../../src/common/mgenMsg.cpp \
	../../../src/common/mgenTransport.cpp \
	../../../src/common/mgenPattern.cpp \
	../../../src/common/mgenPayload.cpp \
	../../../src/common/mgenSequencer.cpp \
	../../../src/common/mgenAppSinkTransport.cpp \
	../../../src/common/mgenApp.cpp
include $(BUILD_EXECUTABLE)

$(call import-module,protolib/makefiles/android/jni)
