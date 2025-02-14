// Copyright (c) 2023 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "paddle/phi/backends/device_ext.h"
#include "runtime/mps_runtime.h"

#define MEMORY_FRACTION 0.5f

C_Status Init() { return C_SUCCESS; }

C_Status InitDevice(const C_Device device) {
  return mps::init_device() ? C_SUCCESS : C_FAILED;
}

C_Status SetDevice(const C_Device device) { return C_SUCCESS; }

C_Status GetDevice(const C_Device device) {
  device->id = 0;
  return C_SUCCESS;
}

C_Status DestroyDevice(const C_Device device) { return C_SUCCESS; }

C_Status Finalize() { return C_SUCCESS; }

C_Status GetDevicesCount(size_t* count) {
  *count = 1;
  return C_SUCCESS;
}

C_Status GetDevicesList(size_t* devices) {
  devices[0] = 0;
  return C_SUCCESS;
}

C_Status MemCpyH2D(const C_Device device,
                   void* dst,
                   const void* src,
                   size_t size) {
  return mps::memcpy_h2d(dst, src, size) ? C_SUCCESS : C_FAILED;
}

C_Status MemCpyD2H(const C_Device device,
                   void* dst,
                   const void* src,
                   size_t size) {
  return mps::memcpy_d2h(dst, src, size) ? C_SUCCESS : C_FAILED;
}

C_Status MemCpyD2D(const C_Device device,
                   void* dst,
                   const void* src,
                   size_t size) {
  return mps::memcpy_d2d(dst, src, size) ? C_SUCCESS : C_FAILED;
}

C_Status AsyncMemCpy(const C_Device device,
                     C_Stream stream,
                     void* dst,
                     const void* src,
                     size_t size) {
  return mps::memcpy_h2d(dst, src, size) ? C_SUCCESS : C_FAILED;
}

C_Status HostAllocate(const C_Device device, void** ptr, size_t size) {
  auto data = malloc(size);
  if (data) {
    *ptr = data;
    return C_SUCCESS;
  } else {
    *ptr = nullptr;
  }
  return C_FAILED;
}

C_Status DeviceAllocate(const C_Device device, void** ptr, size_t size) {
  return mps::alloc_memory(ptr, size) ? C_SUCCESS : C_FAILED;
}

C_Status HostDeallocate(const C_Device device, void* ptr, size_t size) {
  free(ptr);
  return C_SUCCESS;
}

C_Status DeviceDeallocate(const C_Device device, void* ptr, size_t size) {
  return mps::dealloc_memory(ptr) ? C_SUCCESS : C_FAILED;
}

C_Status CreateStream(const C_Device device, C_Stream* stream) {
  stream = nullptr;
  return C_SUCCESS;
}

C_Status DestroyStream(const C_Device device, C_Stream stream) {
  return C_SUCCESS;
}

C_Status CreateEvent(const C_Device device, C_Event* event) {
  return C_SUCCESS;
}

C_Status RecordEvent(const C_Device device, C_Stream stream, C_Event event) {
  return C_SUCCESS;
}

C_Status DestroyEvent(const C_Device device, C_Event event) {
  return C_SUCCESS;
}

C_Status SyncDevice(const C_Device device) { return C_SUCCESS; }

C_Status SyncStream(const C_Device device, C_Stream stream) {
  return C_SUCCESS;
}

C_Status SyncEvent(const C_Device device, C_Event event) { return C_SUCCESS; }

C_Status StreamWaitEvent(const C_Device device,
                         C_Stream stream,
                         C_Event event) {
  return C_SUCCESS;
}

C_Status VisibleDevices(size_t* devices) { return C_SUCCESS; }

C_Status DeviceMemStats(const C_Device device,
                        size_t* total_memory,
                        size_t* free_memory) {
  float memusage;
  FILE* fp;
  char buffer[1024];
  size_t byte_read;
  char* pos;

  fp = fopen("/proc/meminfo", "r");
  byte_read = fread(buffer, 1, sizeof(buffer), fp);
  fclose(fp);
  buffer[byte_read] = '\0';
  pos = strstr(buffer, "MemTotal:");
  sscanf(pos, "MemTotal: %lu kB", total_memory);
  pos = strstr(pos, "MemFree:");
  sscanf(pos, "MemFree: %lu kB", free_memory);
  *total_memory = *total_memory * 1024;
  *free_memory = *free_memory * 1024;
  *free_memory = *free_memory * MEMORY_FRACTION;

  return C_SUCCESS;
}

C_Status DeviceMinChunkSize(const C_Device device, size_t* size) {
  *size = 512;
  return C_SUCCESS;
}

void InitPlugin(CustomRuntimeParams* params) {
  PADDLE_CUSTOM_RUNTIME_CHECK_VERSION(params);
  params->device_type = "mps";
  params->sub_device_type = "v0.1";

  memset(
      reinterpret_cast<void*>(params->interface), 0, sizeof(C_DeviceInterface));

  params->interface->initialize = Init;
  params->interface->finalize = Finalize;

  params->interface->init_device = InitDevice;
  params->interface->set_device = SetDevice;
  params->interface->get_device = GetDevice;
  params->interface->deinit_device = DestroyDevice;

  params->interface->create_stream = CreateStream;
  params->interface->destroy_stream = DestroyStream;

  params->interface->create_event = CreateEvent;
  params->interface->destroy_event = DestroyEvent;
  params->interface->record_event = RecordEvent;

  params->interface->synchronize_device = SyncDevice;
  params->interface->synchronize_stream = SyncStream;
  params->interface->synchronize_event = SyncEvent;
  params->interface->stream_wait_event = StreamWaitEvent;

  params->interface->memory_copy_h2d = MemCpyH2D;
  params->interface->memory_copy_d2d = MemCpyD2D;
  params->interface->memory_copy_d2h = MemCpyD2H;
  params->interface->device_memory_allocate = DeviceAllocate;
  params->interface->host_memory_allocate = HostAllocate;
  params->interface->device_memory_deallocate = DeviceDeallocate;
  params->interface->host_memory_deallocate = HostDeallocate;

  params->interface->get_device_count = GetDevicesCount;
  params->interface->get_device_list = GetDevicesList;
  params->interface->device_memory_stats = DeviceMemStats;
  params->interface->device_min_chunk_size = DeviceMinChunkSize;
}
