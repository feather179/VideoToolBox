#include "Log.h"

#include <cstdarg>
#include <ctime>
#include <chrono>
#include <mutex>
#include <cstdio>

#include <windows.h>

static std::mutex logMutex;
static char buf[100];

void LOGD(const char *fmt, ...) {
#if 1
    std::unique_lock<std::mutex> lock(logMutex);
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));

    printf("[T%5d][%s.%03lld] ", GetCurrentThreadId(), buf, duration.count() % 1000);
    va_list arg;
    va_start(arg, fmt);
    vprintf(fmt, arg);
    va_end(arg);
#endif
}

void LOGE(const char *fmt, ...) {
#if 1
    std::unique_lock<std::mutex> lock(logMutex);
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));

    printf("[T%5d][%s.%03lld] ", GetCurrentThreadId(), buf, duration.count() % 1000);
    va_list arg;
    va_start(arg, fmt);
    vprintf(fmt, arg);
    va_end(arg);
#endif
}
