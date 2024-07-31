#include <pmath-core/numbers.h>

#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/helpers.h>
#include <pmath-util/messages.h>
#include <pmath-util/option-helpers.h>

#include <pmath-builtins/all-symbols-private.h>
#include <pmath-util/approximate.h>

#include <math.h>


extern pmath_symbol_t pmath_System_List;
extern pmath_symbol_t pmath_System_TimeZone;

static const unsigned int DaysIn4Years   =      366 +   3 * 365;
static const unsigned int DaysIn100Years = 24 * 366 +  76 * 365;
static const unsigned int DaysIn400Years = 97 * 366 + 303 * 365;

static const int UnixTime2001 = 978307200; // seconds from 1970-01-01 to 2001-01-01

static const unsigned int NonLeapDaysBeforeMonth[] = {
  0, /* ignored */
  0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

static const unsigned int NonLeapDaysInMonth[] = {
  0, /* ignored */
  31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

// d400 = days since 1 jan of year 1 (+ 400*x) (beginnig with 0 = 1jan)
static void d400_to_ymd(unsigned int d400, unsigned int *y, unsigned int *m, unsigned int *d) {
  unsigned int p100, p4, p1;
  unsigned int leap_year;
  unsigned int prec_days;
  
  assert(d400 < DaysIn400Years);
  
  p100 = d400 / DaysIn100Years; // can be 4! => 31 dec instead of 1 jan
  *y = 100 * p100;
  d400 = d400 % DaysIn100Years;
  
  p4 = d400 / DaysIn4Years;
  *y += 4 * p4;
  d400 = d400 % DaysIn4Years;
  
  p1 = d400 / 365; // can be 4! => 31 dec instead of 1 jan
  *y +=  p1;
  d400 = d400 % 365;
  
  if(p1 == 4 || p100 == 4) {
    assert(d400 == 0);
    *y -= 1;
    *m = 12;
    *d = 31;
    return;
  }
  
  leap_year = p1 == 3 && (p4 != 24 || p100 == 3);
  *m = (d400 + 50) >> 5;
  prec_days = (NonLeapDaysBeforeMonth[*m] + (*m > 2 && leap_year));
  if(prec_days > d400) {
    /* estimate is too large */
    *m -= 1;
    if(*m == 2 && leap_year)
      prec_days -= 29;
    else
      prec_days -= NonLeapDaysInMonth[*m];
  }
  
  *d = d400 - prec_days + 1;
}

PMATH_PRIVATE pmath_t builtin_datelist(pmath_expr_t expr) {
  double timestamp;
  double days2001;
  double seconds;
  unsigned int hour, minute;
  unsigned int days;
  double years;
  unsigned int ymd_y, ymd_m, ymd_d;
  pmath_t options = PMATH_UNDEFINED;
  
  timestamp = pmath_datetime(); // in UTC
  
  if(pmath_expr_length(expr) >= 1) {
    pmath_t item = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_set_of_options(item)) {
      options = pmath_options_extract(expr, 0);
    }
    else {
      options = pmath_options_extract(expr, 1);
      
      if(!pmath_is_number(item)) {
        pmath_unref(item);
        pmath_unref(options);
        return expr;
      }
      
      timestamp = pmath_number_get_d(item);
    }
    
    pmath_unref(item);
  }
  
  if(!pmath_is_null(options)) {
    pmath_t tz = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_TimeZone, options));
    
    if(!pmath_is_number(tz)) {
      pmath_message(PMATH_NULL, "zone", 1, tz);
      pmath_unref(options);
      return expr;
    }
    
    timestamp += pmath_number_get_d(tz) * 60 * 60;
    pmath_unref(tz);
    pmath_unref(options);
  }
  else
    return expr;
    
  pmath_unref(expr);
  
  timestamp = timestamp - UnixTime2001;
  
  days2001 = floor(timestamp / 24 / 60 / 60);
  seconds = timestamp - days2001 * 24 * 60 * 60;
  years = floor(days2001 / DaysIn400Years);
  
  days = (int)(days2001 - DaysIn400Years * years);
  years = 400 * years + 2001;
  
  d400_to_ymd(days, &ymd_y, &ymd_m, &ymd_d);
  years += ymd_y;
  
  minute = (unsigned int)seconds / 60;
  seconds = seconds - 60 * minute;
  
  hour   = minute / 60;
  minute -= hour * 60;
  
  expr = pmath_build_value("(kIIIIf)",
                           (long long int)years,
                           ymd_m,
                           ymd_d,
                           hour,
                           minute,
                           seconds);
                           
  return expr;
}

PMATH_PRIVATE pmath_t builtin_unixtime(pmath_expr_t expr) {
  /* UnixTime()               = UnixTime(DateList())
     UnixTime({y,m,d,h,m,s})
     UnixTime({y,m,d,h,m})    = UnixTime({y,m,d,h,m,0})
     UnixTime({y,m,d,h})      = UnixTime({y,m,d,h,0,0})
     UnixTime({y,m,d})        = UnixTime({y,m,d,0,0,0})
     UnixTime({y,m})          = UnixTime({y,m,1,0,0,0})
     UnixTime({y})            = UnixTime({y,1,1,0,0,0})
     
     Options:
      TimeZone
   */
  pmath_t options = PMATH_UNDEFINED;
  pmath_t arg     = PMATH_UNDEFINED;
  
  if(pmath_expr_length(expr) >= 1) {
    arg = pmath_expr_get_item(expr, 1);
    
    if(pmath_is_set_of_options(arg)) {
      options = pmath_options_extract(expr, 0);
      pmath_unref(arg);
      arg = PMATH_UNDEFINED;
    }
    else {
      options = pmath_options_extract(expr, 1);
    }
  }
  
  double tz_offset_hours = 0;
  if(!pmath_is_null(options)) {
    pmath_t tz = pmath_evaluate(pmath_option_value(PMATH_NULL, pmath_System_TimeZone, options));
    
    if(!pmath_is_number(tz)) {
      pmath_message(PMATH_NULL, "zone", 1, tz);
      pmath_unref(options);
      pmath_unref(arg);
      return expr;
    }
    
    tz_offset_hours = pmath_number_get_d(tz);
    pmath_unref(tz);
    pmath_unref(options);
  }
  
  double timestamp = 0;
  
  if(pmath_is_expr_of(arg, pmath_System_List)) {
    size_t len = pmath_expr_length(arg);
    
    if(len == 0 || len > 6) goto FAIL_ARG;
    
    pmath_t part = pmath_expr_get_item(arg, 1);
    if(!pmath_is_int32(part)) goto FAIL_ARG;
    
    int64_t year = PMATH_AS_INT32(part); // No pmath_unref(part) necessary.
    int64_t month = 1;
    double day = 1;
    double hour = 0;
    double minute = 0;
    double second = 0;
    
    if(len >= 2) {
      part =  pmath_expr_get_item(arg, 2);
      if(!pmath_is_int32(part)) goto FAIL_ARG;
      
      month = PMATH_AS_INT32(part); // No pmath_unref(part) necessary.
    }
    
    if(len >= 3) {
      part = pmath_set_precision(pmath_expr_get_item(arg, 3), -HUGE_VAL);
      if(!pmath_is_number(part)) goto FAIL_ARG;
      
      day = pmath_number_get_d(part);
      pmath_unref(part);
    }
    
    if(len >= 4) {
      part = pmath_set_precision(pmath_expr_get_item(arg, 4), -HUGE_VAL);
      if(!pmath_is_number(part)) goto FAIL_ARG;
      
      hour = pmath_number_get_d(part);
      pmath_unref(part);
    }
    
    if(len >= 5) {
      part = pmath_set_precision(pmath_expr_get_item(arg, 5), -HUGE_VAL);
      if(!pmath_is_number(part)) goto FAIL_ARG;
      
      minute = pmath_number_get_d(part);
      pmath_unref(part);
    }
    
    if(len >= 6) {
      part = pmath_set_precision(pmath_expr_get_item(arg, 6), -HUGE_VAL);
      if(!pmath_is_number(part)) goto FAIL_ARG;
      
      second = pmath_number_get_d(part);
      pmath_unref(part);
    }
    
    int64_t month0 = month - 1;
    int64_t month0_years = month / 12;
    if(month0 < 0)
      month0_years-= 1;
    
    month0 = month0 - month0_years * 12; // 0 .. 11
    year += month0_years;
    month = month0 + 1; // 1 .. 12
    
    year -= 2001;
    
    int64_t num_400year_cycles = year / 400;
    if(year < 0) {
      num_400year_cycles -= 1;
    }
    year = year - num_400year_cycles * 400; // [0 .. 399]
    day += num_400year_cycles * DaysIn400Years;
    
    int num_100year_cycles = (int)year / 100;
    year = year - num_100year_cycles * 100; // [0 .. 99]
    day += num_100year_cycles * DaysIn100Years;
    
    int num_4year_cycles = (int)year / 4;
    year = year - num_4year_cycles * 4; // [0 .. 24]
    day += num_4year_cycles * DaysIn4Years;
    
    day += year * 365;
    day -= 1; // 1st of a month as index 0
    
    pmath_bool_t is_leap_year = year == 3 && (num_4year_cycles != 24 || num_100year_cycles == 3);
    day += NonLeapDaysBeforeMonth[month] + (month > 2 && is_leap_year);
    
    timestamp = UnixTime2001 + second + 60*(minute + 60*(hour - tz_offset_hours + 24*day));
  }
  else if(pmath_same(arg, PMATH_UNDEFINED)) { // UnixTime()
    timestamp = pmath_datetime();// + tz_offset_hours * 60 * 60;
  }
  else goto FAIL_ARG;
  
  if(isfinite(timestamp)) {
    pmath_unref(arg);
    pmath_unref(options);
    pmath_unref(expr);
    return PMATH_FROM_DOUBLE(timestamp);
  }
FAIL_ARG:
  pmath_message(PMATH_NULL, "arg", 1, arg);
  pmath_unref(options);
  return expr;
}
