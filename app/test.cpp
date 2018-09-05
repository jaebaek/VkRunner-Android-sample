/*
 * VkRunner Android sample
 *
 * Copyright (C) 2018 Google LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <android/log.h>
#include <assert.h>
#include <jni.h>
#include <string.h>

#include "vkrunner/vkrunner.h"
#include "vulkan_wrapper.h"

// Android log function wrappers
static const char* kTAG = "VkRunner";
#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

#define CALL_VK(func)                                                   \
  do {                                                                  \
    if (VK_SUCCESS != (func)) {                                         \
      __android_log_print(ANDROID_LOG_ERROR, "VkCompute ",              \
                          "Vulkan error. File[%s], line[%d]", __FILE__, \
                          __LINE__);                                    \
      assert(false);                                                    \
    }                                                                   \
  } while(0)

#define NELEMS(x)  (sizeof(x) / sizeof((x)[0]))

static VkInstance instance;
static VkDebugReportCallbackEXT callback;
static VkPhysicalDevice physicalDevice;
static VkDevice device;

static void checkLayer(VkLayerProperties *&layerProperties, uint32_t &count) {
  CALL_VK(vkEnumerateInstanceLayerProperties(&count, NULL));
  layerProperties = new VkLayerProperties[count];
  CALL_VK(vkEnumerateInstanceLayerProperties(&count, layerProperties));
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags,
                                                    VkDebugReportObjectTypeEXT
                                                    objType,
                                                    uint64_t obj,
                                                    size_t location,
                                                    int32_t code,
                                                    const char* layerPrefix,
                                                    const char* msg,
                                                    void* userData) {
  LOGE("validation layer (%s): %s", layerPrefix, msg);

  return VK_FALSE;
}

static void createInstance() {
  VkLayerProperties *layerProperties;
  uint32_t layerPropertiesCount;
  checkLayer(layerProperties, layerPropertiesCount);

  // Jaebaek: Copy this code from Android NDK document
  //
  // Make sure the desired instance validation layers are available
  // NOTE:  These are not listed in an arbitrary order.  Threading must be
  //        first, and unique_objects must be last.  This is the order they
  //        will be inserted by the loader.
  const char *instance_layers[] = {
    "VK_LAYER_GOOGLE_threading",
    "VK_LAYER_LUNARG_parameter_validation",
    "VK_LAYER_LUNARG_object_tracker",
    "VK_LAYER_LUNARG_core_validation",
    "VK_LAYER_LUNARG_device_limits",
    "VK_LAYER_LUNARG_image",
    "VK_LAYER_LUNARG_swapchain",
    "VK_LAYER_GOOGLE_unique_objects"
  };

  const char *requiredProperties[NELEMS(instance_layers)];
  int requiredPropertiesCount = 0;
  for (int k = 0; k < NELEMS(instance_layers); ++k) {
    const char *layer = nullptr;
    for (int i = 0; i < layerPropertiesCount; i++) {
      if (!strcmp(instance_layers[k], layerProperties[i].layerName)) {
        layer = instance_layers[k];
        break;
      }
    }
    if (layer != nullptr) {
      LOGI("%s", layer);
      requiredProperties[requiredPropertiesCount] = layer;
      ++requiredPropertiesCount;
    }
  }

  const char *enabledExtension[] = {
    "VK_EXT_debug_report"
  };

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);
  VkResult result;

  VkInstanceCreateInfo instInfo = {};
  instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instInfo.pApplicationInfo = &appInfo;
  instInfo.enabledLayerCount = requiredPropertiesCount;
  instInfo.ppEnabledLayerNames = requiredProperties;
  instInfo.enabledExtensionCount = 1;
  instInfo.ppEnabledExtensionNames = enabledExtension;

  CALL_VK(vkCreateInstance(&instInfo, NULL, &instance));

  VkDebugReportCallbackCreateInfoEXT createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
  createInfo.flags =
      VK_DEBUG_REPORT_ERROR_BIT_EXT |
      VK_DEBUG_REPORT_WARNING_BIT_EXT |
      VK_DEBUG_REPORT_DEBUG_BIT_EXT;
  createInfo.pfnCallback = debugCallback;

  auto func = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(
      instance, "vkCreateDebugReportCallbackEXT");

  CALL_VK(func(instance, &createInfo, nullptr, &callback));

  delete layerProperties;
}

static int getQueueFamilyIndex(const VkPhysicalDevice& targetPhysicalDevice) {
  uint32_t count;
  VkQueueFamilyProperties *properties;

  vkGetPhysicalDeviceQueueFamilyProperties(targetPhysicalDevice, &count, NULL);

  properties = new VkQueueFamilyProperties[count];
  vkGetPhysicalDeviceQueueFamilyProperties(targetPhysicalDevice,
                                           &count,
                                           properties);

  for (uint32_t i = 0;i < count; ++i) {
    if ((properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
        (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
      return i;
    }
  }

  delete properties;

  return -1;
}

static int createPhysicalDevice() {
  uint32_t count;
  VkPhysicalDevice *physicalDevices;
  VkResult result;

  physicalDevice = 0;

  CALL_VK(vkEnumeratePhysicalDevices(instance, &count, NULL));

  physicalDevices = new VkPhysicalDevice[count];
  CALL_VK(vkEnumeratePhysicalDevices(instance, &count, physicalDevices));

  for (uint32_t i = 0;i < count; ++i) {
    VkPhysicalDeviceProperties property;
    vkGetPhysicalDeviceProperties(physicalDevices[i], &property);

    int q = getQueueFamilyIndex(physicalDevices[i]);
    if (q != -1) {
      physicalDevice = physicalDevices[i];
      delete physicalDevices;
      return q;
    }
  }

  LOGW("No device supports both VK_QUEUE_COMPUTE_BIT");
  delete physicalDevices;
  return -1;
}

static void createDevice(int qindex) {
  VkDeviceQueueCreateInfo queueInfo;
  const float priorities[] = { 1.0f };

  queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueInfo.queueFamilyIndex = qindex;
  queueInfo.queueCount = 1;
  queueInfo.pQueuePriorities = priorities;

  VkDeviceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  info.pQueueCreateInfos = &queueInfo;
  info.queueCreateInfoCount = 1;

  CALL_VK(vkCreateDevice(physicalDevice, &info, NULL, &device));
}

static void *getInstanceProcAddr(const char *name,
                                 void *user_data) {
  return (void *) vkGetInstanceProcAddr(instance, name);
}

static void error_cb(const char *message, void *user_data) {
  LOGE("%s", message);
}

static const char *string_scripts[] = {
#include "string_scripts.inc"
};

extern "C" {
JNIEXPORT void JNICALL
Java_com_vkrunner_VkRunner_test( JNIEnv* env, jobject thiz );
}

JNIEXPORT void JNICALL
Java_com_vkrunner_VkRunner_test( JNIEnv* env, jobject thiz ) {
  if (!InitVulkan()) {
    LOGE("Vulkan is unavailable, install vulkan and re-start");
    return;
  }

  struct vr_executor *executor = vr_executor_new();

  createInstance();
  int qindex = createPhysicalDevice();
  createDevice(qindex);

  vr_executor_set_error_cb(executor, error_cb);
  vr_executor_set_device(executor,
                         getInstanceProcAddr,
                         NULL,
                         (void *) physicalDevice,
                         qindex,
                         (void *) device);

  for (int i = 0;i < sizeof(string_scripts) / sizeof(char *); i += 2) {
    struct vr_source *source = vr_source_from_string(string_scripts[i + 1]);
    enum vr_result result = vr_executor_execute(executor, source);
    vr_source_free(source);
    LOGI("result of \"%s\": \"%s\"\n",
         string_scripts[i],
         vr_result_to_string(result));
  }

  vr_executor_free(executor);
}
