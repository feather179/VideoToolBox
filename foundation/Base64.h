#ifndef BASE64_H
#define BASE64_H

#include <cstring>
#include <cstdint>

bool decodeBase64(const char *s, size_t *inOutBufSize, uint8_t *out);
void encodeBase64(const uint8_t *data, size_t size, char *out);

#endif
