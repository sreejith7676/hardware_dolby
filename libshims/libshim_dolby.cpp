//
// SPDX-FileCopyrightText: The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

#define LOG_TAG "DolbyShim"

#include <log/log.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <atomic>

static pthread_mutex_t s_pipe_mutex = PTHREAD_MUTEX_INITIALIZER;
static int s_pfd[2] = {-1, -1};

static bool is_readable(const void* ptr, size_t size) {
    if (!ptr || (uintptr_t)ptr < 4096) return false;
    pthread_mutex_lock(&s_pipe_mutex);
    if (s_pfd[0] == -1) {
        if (pipe2(s_pfd, O_CLOEXEC | O_NONBLOCK) < 0) {
            pthread_mutex_unlock(&s_pipe_mutex);
            return false;
        }
    }
    ssize_t ret = write(s_pfd[1], ptr, size);
    if (ret > 0) {
        char drain[64];
        read(s_pfd[0], drain, ret);
        pthread_mutex_unlock(&s_pipe_mutex);
        return true;
    }
    pthread_mutex_unlock(&s_pipe_mutex);
    return false;
}

static bool is_valid_refbase(const void* thisptr) {
    if (!thisptr || (uintptr_t)thisptr < 4096) {
        return false;
    }
    // RefBase has vtable pointer (8 bytes) + mRefs pointer (8 bytes)
    if (!is_readable(thisptr, sizeof(void*) * 2)) {
        return false;
    }
    const void* mRefs = *(const void**)((uintptr_t)thisptr + sizeof(void*));
    if (!mRefs || (uintptr_t)mRefs < 4096) {
        return false;
    }
    // weakref_impl has mStrong (int32_t) + mWeak (int32_t)
    if (!is_readable(mRefs, sizeof(int32_t) * 2)) {
        return false;
    }
    return true;
}

struct DummyWeakRef {
    std::atomic<int32_t> mStrong{1000000};
    std::atomic<int32_t> mWeak{1000000};
    void* mBase{nullptr};
    std::atomic<int32_t> mFlags{0};
};
static DummyWeakRef s_dummy_weakref;

extern "C" void _ZNK7android7RefBase9incStrongEPKv(void* thisptr, const void* id) {
    if (!is_valid_refbase(thisptr)) {
        ALOGW("DolbyShim: incStrong ignored on invalid/null RefBase or null mRefs (%p)", thisptr);
        return;
    }
    typedef void (*RealFunc)(void*, const void*);
    static RealFunc real = (RealFunc)dlsym(RTLD_NEXT, "_ZNK7android7RefBase9incStrongEPKv");
    if (real) real(thisptr, id);
}

extern "C" void _ZNK7android7RefBase9decStrongEPKv(void* thisptr, const void* id) {
    if (!is_valid_refbase(thisptr)) {
        ALOGW("DolbyShim: decStrong ignored on invalid/null RefBase or null mRefs (%p)", thisptr);
        return;
    }
    typedef void (*RealFunc)(void*, const void*);
    static RealFunc real = (RealFunc)dlsym(RTLD_NEXT, "_ZNK7android7RefBase9decStrongEPKv");
    if (real) real(thisptr, id);
}

extern "C" void* _ZNK7android7RefBase10createWeakEPKv(void* thisptr, const void* id) {
    if (!is_valid_refbase(thisptr)) {
        ALOGW("DolbyShim: createWeak on invalid/null RefBase or null mRefs (%p), returning dummy weakref", thisptr);
        return &s_dummy_weakref;
    }
    typedef void* (*RealFunc)(void*, const void*);
    static RealFunc real = (RealFunc)dlsym(RTLD_NEXT, "_ZNK7android7RefBase10createWeakEPKv");
    return real ? real(thisptr, id) : &s_dummy_weakref;
}

extern "C" void _ZN7android7RefBase12weakref_type7incWeakEPKv(void* thisptr, const void* id) {
    if (!thisptr || (uintptr_t)thisptr < 4096 || !is_readable(thisptr, sizeof(int32_t) * 2)) {
        ALOGW("DolbyShim: incWeak ignored on invalid pointer (%p)", thisptr);
        return;
    }
    typedef void (*RealFunc)(void*, const void*);
    static RealFunc real = (RealFunc)dlsym(RTLD_NEXT, "_ZN7android7RefBase12weakref_type7incWeakEPKv");
    if (real) real(thisptr, id);
}

extern "C" void _ZN7android7RefBase12weakref_type7decWeakEPKv(void* thisptr, const void* id) {
    if (!thisptr || (uintptr_t)thisptr < 4096 || !is_readable(thisptr, sizeof(int32_t) * 2)) {
        ALOGW("DolbyShim: decWeak ignored on invalid pointer (%p)", thisptr);
        return;
    }
    typedef void (*RealFunc)(void*, const void*);
    static RealFunc real = (RealFunc)dlsym(RTLD_NEXT, "_ZN7android7RefBase12weakref_type7decWeakEPKv");
    if (real) real(thisptr, id);
}

extern "C" bool _ZN7android7RefBase12weakref_type16attemptIncStrongEPKv(void* thisptr, const void* id) {
    if (!thisptr || (uintptr_t)thisptr < 4096 || !is_readable(thisptr, sizeof(int32_t) * 2)) {
        ALOGW("DolbyShim: attemptIncStrong ignored on invalid pointer (%p)", thisptr);
        return false;
    }
    typedef bool (*RealFunc)(void*, const void*);
    static RealFunc real = (RealFunc)dlsym(RTLD_NEXT, "_ZN7android7RefBase12weakref_type16attemptIncStrongEPKv");
    return real ? real(thisptr, id) : false;
}
