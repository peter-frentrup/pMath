#ifndef PMATH__LANGUAGE__SOURCE_LOCATION_H__INCLUDED
#define PMATH__LANGUAGE__SOURCE_LOCATION_H__INCLUDED

#include <pmath-core/objects.h>

/** \defgroup location_tracking Source Location Tracking
    \ingroup parsing_code 
    \brief Trace expressions to their original source code location.
    
    The pMath parser automatically tracks source locations of expressions. That
    is done by first attaching a <tt>Language`SourceLocation(...)</tt> to parsed boxes
    and tokens via pmath_boxes_from_spans_ex_t::add_debug_metadata in pmath_boxes_from_spans_ex().
    Afterwards, <tt>MakeExpression(...)</tt> forwards these attached metadata from 
    the boxes and tokens to the final expressions.
  @{
 */

/** \brief Create a <tt>Language`SourceLocation(...)</tt> for a text file.
    \param ref An arbitrary object. Usually a file name (string). It will be freed.
    \param startline 1-based start line.
    \param startcol  0-based character offset within the start line (UTF-16 code point offset).
    \param endline   1-based end line.
    \param endcol    0-based character offset within the end line (UTF-16 code point offset).
    \return A new expression of the form <tt>Language`SourceLocation(\a ref, {\a startline, \a startcol} .. {\a endline, \a endcol})</tt>.
 */
PMATH_API
pmath_t pmath_language_new_file_location(pmath_t ref, int startline, int startcol, int endline, int endcol);


/** \brief Create a <tt>Language`SourceLocation(...)</tt> for a simple range.
    \param ref An arbitrary object. Usually a <tt>FrontEndObject(...)</tt>. It will be freed.
    \param start  0-based character offset (usually UTF-16 code point offset).
    \param end    0-based character offset (usually UTF-16 code point offset).
    \return A new expression of the form <tt>Language`SourceLocation(\a ref, \a start .. \a end)</tt>.
 */
PMATH_API
pmath_t pmath_language_new_simple_location(pmath_t ref, int start, int end);

/** @} */

#endif // PMATH__LANGUAGE__SOURCE_LOCATION_H__INCLUDED
