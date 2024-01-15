#ifndef MD5_H
#define MD5_H

#include <cstdint>
#include <string>

void md5Sum(uint8_t *dst, const uint8_t *src, size_t len);
std::string md5Sum(const uint8_t *src, size_t len);

#endif
