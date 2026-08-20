#pragma once
// ---------------------------------------------------------------------------
// win_mutex.hpp
//
// Thin RAII wrapper around Windows CRITICAL_SECTION that presents the same
// interface as std::mutex / std::lock_guard.
//
// Required because MinGW.org GCC 6.3 does not provide a working <mutex>
// implementation. On Linux / MinGW-w64 builds std::mutex is used directly;
// this header is only compiled on MinGW.org (detected via _WIN32 without
// _GLIBCXX_HAS_GTHREADS being defined by the GCC build).
// ---------------------------------------------------------------------------

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
