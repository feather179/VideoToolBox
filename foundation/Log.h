#ifndef LOG_H
#define LOG_H

#if defined(_MSC_VER)
#    define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

void LOGD(const char *fmt, ...);
void LOGE(const char *fmt, ...);

#endif // LOG_H
