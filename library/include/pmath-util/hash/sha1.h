#ifndef PMATH_UTIL__HASH__SHA1_H__INCLUDED
#define PMATH_UTIL__HASH__SHA1_H__INCLUDED


#ifndef BUILDING_PMATH
#  error This header file is not part of the public pMath API
#endif

#include <pmath-config.h>

#include <stdint.h>


/* SHA-1 in C
   By Steve Reid <steve@edmweb.com>
   Public Domain
 */
struct _pmath_sha1_context_t {
  uint32_t state[5];
  uint32_t count[2];
  uint8_t buffer[64];
};

struct _pmath_sha1_digest_t {
  uint8_t digest[20];
};

PMATH_PRIVATE
void _pmath_sha1_transform(uint32_t state[5], const uint8_t buffer[64]);

PMATH_PRIVATE
void _pmath_sha1_init(struct _pmath_sha1_context_t *context);

PMATH_PRIVATE
void _pmath_sha1_update(struct _pmath_sha1_context_t *context, const uint8_t *data, uint32_t len);

PMATH_PRIVATE
void _pmath_sha1_final(struct _pmath_sha1_digest_t hash_out[1], struct _pmath_sha1_context_t *context);

PMATH_PRIVATE
void _pmath_sha1(struct _pmath_sha1_digest_t hash_out[1], const uint8_t *buf, size_t len);

#endif
