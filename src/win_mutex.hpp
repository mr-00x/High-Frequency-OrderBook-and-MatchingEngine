#pragma once
// ---------------------------------------------------------------------------
// win_mutex.hpp (Cross-platform mutex abstraction)
//
// On Windows with old MinGW, uses Windows CRITICAL_SECTION.
// On modern Linux/Unix/GCC/Clang, uses standard std::mutex / std::lock_guard.
// ---------------------------------------------------------------------------

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace hft {

class WinMutex {
public:
    WinMutex()  { InitializeCriticalSection(&cs_); }
    ~WinMutex() { DeleteCriticalSection(&cs_); }

    WinMutex(const WinMutex&)            = delete;
    WinMutex& operator=(const WinMutex&) = delete;

    void lock()   { EnterCriticalSection(&cs_); }
    void unlock() { LeaveCriticalSection(&cs_); }

private:
    CRITICAL_SECTION cs_;
};

template<typename Mutex>
class LockGuard {
public:
    explicit LockGuard(Mutex& m) : m_(m) { m_.lock(); }
    ~LockGuard()                          { m_.unlock(); }

    LockGuard(const LockGuard&)            = delete;
    LockGuard& operator=(const LockGuard&) = delete;

private:
    Mutex& m_;
};

} // namespace hft

#else // POSIX / Linux / macOS

#include <mutex>

namespace hft {
    using WinMutex = std::mutex;
    template<typename M>
    using LockGuard = std::lock_guard<M>;
} // namespace hft

#endif
