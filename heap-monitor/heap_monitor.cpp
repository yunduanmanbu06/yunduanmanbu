// Copyright 2017 Baidu Inc. All Rights Reserved.
// Author: Zhong Shuai (zhongshuai@baidu.com)
//
// Description: Heap Monitor

#ifdef HEAP_MONITOR

#include "heap_monitor.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "rtos.h"

#define HM_LOG(...)     mbed_error_printf(__VA_ARGS__)

struct mem_block_info {
    uint32_t magic;
    size_t size;
    enum Module module;
};

struct heap_info {
    uint64_t max_used_heap_size;
    uint64_t current_used_heap_size;
};

static const uint32_t MAGIC = 0x99ee;
static const uint64_t HEAP_SIZE = 160 * 1024;
static const size_t HEAD_SIZE = sizeof(struct mem_block_info);

static rtos::Mutex lock;

static struct heap_info heap_info[MAX_MODULE];

static const char* const module_name[MAX_MODULE] = {
    "OS       ",
    "APP      ",
    "OTA      ",
    "CA       ",
    "HTTP     ",
    "MEDIA    ",
    "RECORDER ",
    "SPEEX_LIB",
    "UTILITY  ",
    "UPNP     ",
    "ALARM    "
};

void init_heap_info(void) {
    HM_LOG("\nInit heap info\n");
    lock.lock();
    memset(&heap_info, 0, sizeof(heap_info));
    lock.unlock();
}

void show_heap_info(void) {
    int i = 0;
    uint64_t sum = 0;
    uint64_t max_used_heap_size = 0;
    uint64_t current_used_heap_size = 0;

    HM_LOG("\n================ Heap Info ================\n");
    HM_LOG("Module\t\t\t\tMax Heap Size\t\tCurrent Heap Size\n");

    for (i = 0, sum = 0; i < MAX_MODULE; i++) {
        lock.lock();
        max_used_heap_size = heap_info[i].max_used_heap_size;
        current_used_heap_size = heap_info[i].current_used_heap_size;
        lock.unlock();
        HM_LOG("%s\t\t%10lld Byte\t\t%10lld Byte\n",
                module_name[i],
                max_used_heap_size,
                current_used_heap_size);
        sum += current_used_heap_size;
    }

    HM_LOG("Used: %lld Byte\nFree: %lld Byte\n", sum, HEAP_SIZE - sum);
    HM_LOG("============================================\n");
}

void* malloc_t(size_t size, enum Module module) {
    void* buf = NULL;
    char* head = NULL;
    struct mem_block_info* block_info = NULL;

    if (module < OS || module >= MAX_MODULE) {
        return NULL;
    }

    buf = malloc(size + HEAD_SIZE);
    if (!buf) {
        return NULL;
    }

    head = (char*)buf;
    buf = (void*)(head + HEAD_SIZE);
    block_info = (struct mem_block_info*)head;
    block_info->magic = MAGIC;
    block_info->size = size;
    block_info->module = module;
    lock.lock();
    heap_info[module].current_used_heap_size += size;

    if (heap_info[module].current_used_heap_size >
            heap_info[module].max_used_heap_size) {
        heap_info[module].max_used_heap_size = heap_info[module].current_used_heap_size;
    }

    lock.unlock();
    return buf;
}

void* calloc_t(size_t nmemb, size_t size, enum Module module) {
    void* buf = NULL;
    char* head = NULL;
    size_t block_size = 0;
    struct mem_block_info* block_info = NULL;

    if (nmemb <= 0 || size <= 0
            || module < OS
            || module > MAX_MODULE) {
        return NULL;
    }

    block_size = (nmemb * size) + HEAD_SIZE;
    head = (char*)calloc(1, block_size);

    if (!head) {
        return NULL;
    }

    buf = (void*)(head + HEAD_SIZE);
    block_info = (struct mem_block_info*)head;
    block_info->magic = MAGIC;
    block_info->size = block_size - HEAD_SIZE;
    block_info->module = module;
    lock.lock();
    heap_info[module].current_used_heap_size += block_size - HEAD_SIZE;

    if (heap_info[module].current_used_heap_size >
            heap_info[module].max_used_heap_size) {
        heap_info[module].max_used_heap_size = heap_info[module].current_used_heap_size;
    }

    lock.unlock();
    return buf;
}

void* realloc_t(void* ptr, size_t size, enum Module module) {
    char* p = NULL;
    char* head = NULL;
    void* buf = NULL;
    size_t block_size = 0;
    struct mem_block_info* block_info = NULL;

    if (!ptr) {
        if (size > 0) {
            buf = malloc_t(size, module);
            return buf;
        } else {
            return NULL;
        }
    }

    if (ptr != NULL && size == 0) {
        free_t(ptr);
        return NULL;
    }

    head = (char*)ptr;
    head -= HEAD_SIZE;
    block_info = (struct mem_block_info*)head;

    if (block_info->magic == MAGIC) {
        if (block_info->module != module) {
            HM_LOG("Heap Monitor: Module error\n");
            return NULL;
        }

        block_size = block_info->size;
        buf = realloc(head, size + HEAD_SIZE);

        if (!buf) {
            return NULL;
        } else {
            block_info = (struct mem_block_info*)buf;
            block_info->magic = MAGIC;
            block_info->size = size;
            block_info->module = module;
            lock.lock();

            if (size > block_size) {
                heap_info[module].current_used_heap_size += size - block_size;
            }

            if (heap_info[module].current_used_heap_size >
                    heap_info[module].max_used_heap_size) {
                heap_info[module].max_used_heap_size = heap_info[module].current_used_heap_size;
            }

            lock.unlock();
            p = (char*)buf;
            p += HEAD_SIZE;
            buf = (void*)p;
            return buf;
        }
    } else {
#ifdef DEBUG_HEAP_MONITOR
        HM_LOG("Heap Monitor: Magic error\n");
#endif
        return NULL;
    }
}

void free_t(void* ptr) {
    char* head = NULL;
    enum Module module;
    struct mem_block_info* block_info = NULL;

    if (!ptr) {
        return;
    }

    head = (char*)ptr;
    head -= HEAD_SIZE;
    block_info = (struct mem_block_info*)head;

    if (block_info->magic == MAGIC) {
        module = block_info->module;

        if (module >= OS && module < MAX_MODULE) {
            lock.lock();
            heap_info[module].current_used_heap_size -= block_info->size;
            lock.unlock();
        }

        free(head);
    } else {
#ifdef DEBUG_HEAP_MONITOR
        HM_LOG("Heap Monitor: magic error\n");
#endif
        free(ptr);
    }
}

void* operator new (std::size_t size) {
    void* buf = NULL;
    buf = malloc_t(size, OS);
    return buf;
}

void* operator new[](std::size_t size) {
    void* buf = NULL;
    buf = malloc_t(size, OS);
    return buf;
}

void* operator new (std::size_t size, int module) {
    void* buf = NULL;
    buf = malloc_t(size, (enum Module)module);
    return buf;
}

void* operator new[](std::size_t size, int module) {
    void* buf = NULL;
    buf = malloc_t(size, (enum Module)module);
    return buf;
}

void operator delete (void* ptr) {
    if (ptr) {
        free_t(ptr);
    }
}

void operator delete[](void* ptr) {
    if (ptr) {
        free_t(ptr);
    }
}

#endif // HEAP_MONITOR

