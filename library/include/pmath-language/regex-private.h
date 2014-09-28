#ifndef __PMATH_LANGUAGE__REGEX_PRIVATE_H__
#define __PMATH_LANGUAGE__REGEX_PRIVATE_H__

#include <pmath-core/strings.h>

struct _capture_t {
  int *ovector;
  int ovecsize;
  int capture_max;
};

struct _regex_t;

PMATH_PRIVATE pmath_bool_t _pmath_regex_init_capture(
  const struct _regex_t *re,
  struct _capture_t     *c);

PMATH_PRIVATE void _pmath_regex_free_capture(struct _capture_t *c);

PMATH_PRIVATE struct _regex_t *_pmath_regex_ref(struct _regex_t *re);
PMATH_PRIVATE void _pmath_regex_unref(struct _regex_t *re);

/* Compile a regular expression with some options for pcre_compile2.

   On failure, a message is generated and PMATH_NULL will be returend.

   Free the result with _pmath_regex_unref() when it is no longer used.
 */
PMATH_PRIVATE struct _regex_t *_pmath_regex_compile(
  pmath_t obj,            // will be freed
  int     pcre_options);

/* Match a regular expression to a subject string. Matching starts at subject_offset.

   Additional pcre_options can be given for pcre_exec.

   If the regex matches, capture->ovector[0] contains the offset of the
   match and capture->ovector[1] the length of the match.

   Optionally, a right-hand-side "rhs" can be adjusted as needed by
   StringReplace/...
 */
PMATH_PRIVATE pmath_bool_t _pmath_regex_match(
  struct _regex_t    *regex,
  pmath_string_t      subject,        // wont be freed
  int                 subject_offset,
  int                 pcre_options,
  struct _capture_t  *capture,        // must be initialized with _pmath_regex_init_capture()
  pmath_t            *rhs);           // rhs is optional


PMATH_PRIVATE void _pmath_regex_memory_panic(void);

PMATH_PRIVATE pmath_bool_t _pmath_regex_init(void);
PMATH_PRIVATE void         _pmath_regex_done(void);

#endif /* __PMATH_LANGUAGE__REGEX_PRIVATE_H__ */
