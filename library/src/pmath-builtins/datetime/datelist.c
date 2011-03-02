#include <pmath-core/numbers.h>

#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-builtins/control-private.h>

#include <math.h>


static const unsigned int DaysIn4Years   =      366 +   3 * 365;
static const unsigned int DaysIn100Years = 24 * 366 +  76 * 365;
static const unsigned int DaysIn400Years = 97 * 366 + 303 * 365;

static const unsigned int NonLeapDaysBeforeMonth[] = {
    0, /* ignored */
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

static const unsigned int NonLeapDaysInMonth[] = {
    0, /* ignored */
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

// d400 = days since 1 jan of year 1 (+ 400*x) (beginnig with 0 = 1jan)
static void d400_to_ymd(unsigned int d400, unsigned int *y, unsigned int *m, unsigned int *d){
  unsigned int p100, p4, p1;
  unsigned int leap_year;
  unsigned int prec_days;
  
  assert(d400 < DaysIn400Years);
  
  p100 = d400 / DaysIn100Years; // can be 4! => 31 dec instead of 1 jan
  *y = 100 * p100;
  d400 = d400 % DaysIn100Years;
  
  p4 = d400 / DaysIn4Years;
  *y+= 4 * p4;
  d400 = d400 % DaysIn4Years;
  
  p1 = d400 / 365; // can be 4! => 31 dec instead of 1 jan
  *y+=  p1;
  d400 = d400 % 365;
  
  if(p1 == 4 || p100 == 4){
    assert(d400 == 0);
    *y -= 1;
    *m = 12;
    *d = 31;
    return;
  }
  
  leap_year = p1 == 3 && (p4 != 24 || p100 == 3);
  *m = (d400 + 50) >> 5;
  prec_days = (NonLeapDaysBeforeMonth[*m] + (*m > 2 && leap_year));
  if(prec_days > d400){
    /* estimate is too large */
    *m -= 1;
    if(*m == 2 && leap_year)
      prec_days-= 29;
    else
      prec_days-= NonLeapDaysInMonth[*m];
  }
  
  *d = d400 - prec_days + 1;
}


PMATH_PRIVATE pmath_t builtin_datelist(pmath_expr_t expr){
  double timestamp;
  double days2001;
  double seconds;
  unsigned int hour, minute;
  unsigned int days;
  double years;
  unsigned int ymd_y, ymd_m, ymd_d;
  static const int seconds_until_2001 = 978307200; // seconds from 1970-01-01 to 2001-01-01
  static const int days_per_400years = 97 * 366 + 303 * 365;
  pmath_t options = PMATH_UNDEFINED;
  
  timestamp = pmath_tickcount();
  
  if(pmath_expr_length(expr) > 0){
    pmath_t item = pmath_expr_get_item(expr, 1);
    
    if(_pmath_is_rule(item) || _pmath_is_list_of_rules(item)){
      options = pmath_options_extract(expr, 0);
    }
    else{
      options = pmath_options_extract(expr, 1);
      
      if(!pmath_is_number(item)){
        pmath_unref(item);
        pmath_unref(options);
        return expr;
      }
      
      timestamp = pmath_number_get_d(item);
    }
    
    pmath_unref(item);
  }
  
  if(!pmath_is_null(options)){
    pmath_t tz = pmath_evaluate(pmath_option_value(PMATH_NULL, PMATH_SYMBOL_TIMEZONE, options));
    
    if(!pmath_is_number(tz)){
      pmath_unref(tz);
      return expr;
    }
    
    timestamp-= pmath_number_get_d(tz) * 60 * 60;
    pmath_unref(tz);
    pmath_unref(options);
  }
  else
    return expr;
  
  pmath_unref(expr);
  
  timestamp = timestamp - seconds_until_2001;
  
  days2001 = floor(timestamp / 24 / 60 / 60);
  seconds = timestamp - days2001 * 24 * 60 * 60;
  years = floor(days2001 / days_per_400years);
  
  days = (int)(days2001 - days_per_400years * years);
  years = 400 * years + 2001;
  
  d400_to_ymd(days, &ymd_y, &ymd_m, &ymd_d);
  years+= ymd_y;
  
  minute = (unsigned int)seconds / 60;
  seconds = seconds - 60 * minute;
  
  hour   = minute / 60;
  minute-= hour * 60;
  
  expr = pmath_build_value("(kIIIIf)", 
    (long long int)years,
    ymd_m,
    ymd_d,
    hour,
    minute,
    seconds);
  
  return expr;
}
