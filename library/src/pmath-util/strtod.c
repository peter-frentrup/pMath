/* original source: http://svn.ruby-lang.org/repos/ruby/branches/ruby_1_8/missing/strtod.c
 *
 *  Source code for the "strtod" library procedure.
 *
 * Copyright (c) 1988-1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose. It is provided "as is" without
 * express or implied warranty.
 *
 */

#include <pmath-types.h>
#include <pmath-util/strtod.h>


static int maxExponent = 511;  /* Largest possible base 10 exponent.  Any
                                * exponent larger than this will already
                                * produce underflow or overflow, so there's
                                * no need to worry about additional digits.
                                */
static double powersOf10[] = { /* Table giving binary powers of 10.  Entry */
  10.,                         /* is 10^2^i.  Used to convert decimal */
  100.,                        /* exponents into floating-point numbers. */
  1.0e4,
  1.0e8,
  1.0e16,
  1.0e32,
  1.0e64,
  1.0e128,
  1.0e256
};

PMATH_API
double pmath_strtod(
  const char  *str,
  const char **end
) {
  int sign, expSign = FALSE;
  double fraction, dblExp, *d;
  register const char *p;
  register int c;
  int exp = 0;        /* Exponent read from "EX" field. */
  int fracExp = 0;    /* Exponent that derives from the fractional
                       * part.  Under normal circumstatnces, it is
                       * the negative of the number of digits in F.
                       * However, if I is very long, the last digits
                       * of I get dropped (otherwise a long I with a
                       * large negative exponent could cause an
                       * unnecessary overflow on I alone).  In this
                       * case, fracExp is incremented one for each
                       * dropped digit. */
  int mantSize;       /* Number of digits in mantissa. */
  int decPt;          /* Number of mantissa digits BEFORE decimal
                       * point. */
  const char *pExp;   /* Temporarily holds location of exponent in str. */
  
  /*
   * Strip off leading blanks and check for a sign.
   */
  
  p = str;
  while(*p <= ' ')
    ++p;
    
  if(*p == '-') {
    sign = TRUE;
    ++p;
  }
  else {
    if(*p == '+')
      ++p;
      
    sign = FALSE;
  }
  
  /*
   * Count the number of digits in the mantissa (including the decimal
   * point), and also locate the decimal point.
   */
  
  decPt = -1;
  for(mantSize = 0;; mantSize += 1) {
    c = *p;
    if(c < '0' || c > '9') {
      if(c != '.' || decPt >= 0)
        break;
        
      decPt = mantSize;
    }
    
    ++p;
  }
  
  /*
   * Now suck up the digits in the mantissa.  Use two integers to
   * collect 9 digits each (this is faster than using floating-point).
   * If the mantissa has more than 18 digits, ignore the extras, since
   * they can't affect the value anyway.
   */
  
  pExp  = p;
  p -= mantSize;
  if(decPt < 0)
    decPt = mantSize;
  else
    --mantSize;   /* One of the digits was the point. */
    
  if(mantSize > 18) {
    fracExp = decPt - 18;
    mantSize = 18;
  }
  else {
    fracExp = decPt - mantSize;
  }
  
  if(mantSize == 0) {
    fraction = 0.0;
    p = str;
    goto DONE;
  }
  else {
    int frac1, frac2;
    frac1 = 0;
    for(; mantSize > 9; --mantSize) {
      c = *p++;
      if(c == '.')
        c = *p++;
        
      frac1 = 10 * frac1 + (c - '0');
    }
    
    frac2 = 0;
    for(; mantSize > 0; --mantSize) {
      c = *p++;
      if(c == '.')
        c = *p++;
        
      frac2 = 10 * frac2 + (c - '0');
    }
    
    fraction = (1.0e9 * frac1) + frac2;
  }
  
  /*
   * Skim off the exponent.
   */
  
  p = pExp;
  if(*p == 'E' || *p == 'e') {
    ++p;
    if(*p == '-') {
      expSign = TRUE;
      ++p;
    }
    else {
      if(*p == '+')
        ++p;
        
      expSign = FALSE;
    }
    
    while(*p >= '0' && *p <= '9') {
      exp = exp * 10 + (*p - '0');
      ++p;
    }
  }
  
  if(expSign)
    exp = fracExp - exp;
  else
    exp = fracExp + exp;
    
    
  /*
   * Generate a floating-point number that represents the exponent.
   * Do this by processing the exponent one bit at a time to combine
   * many powers of 2 of 10. Then combine the exponent with the
   * fraction.
   */
  
  if(exp < 0) {
    expSign = TRUE;
    exp = -exp;
  }
  else
    expSign = FALSE;
    
  if(exp > maxExponent) {
    exp = maxExponent;
    //errno = ERANGE;
  }
  
  dblExp = 1.0;
  for(d = powersOf10; exp != 0; exp >>= 1, d += 1) {
    if(exp & 01)
      dblExp *= *d;
  }
  
  if(expSign)
    fraction /= dblExp;
  else
    fraction *= dblExp;
    
DONE:
  if(end)
    *end = p;
    
  if(sign)
    return -fraction;
    
  return fraction;
}
