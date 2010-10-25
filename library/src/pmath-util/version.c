#include <pmath-util/version.h>

#include <stdlib.h>
#include <string.h>


static const char *compile_datetime[] = {
  // compile-datetime.inc always contains  __DATE__,__TIME__
  // but its timestamp is updated before every build via 
  // pre-build-[windows.bat|linux.sh], so this file will also recompile every 
  // time.
  #include "version-datetime.inc"
};

PMATH_API
void pmath_version_get_datetime(
  int *year,
  int *month,
  int *day,
  int *hour,
  int *minute,
  int *second
){
  // ISO C, section 6.8.8:
  // __DATE__ has format "Mmm dd yyyy"
  // __TIME__ has format "hh:mm:ss"
  static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char date[12];
  char time[9];
  int i;
  
  memcpy(date, compile_datetime[0], sizeof(date));
  memcpy(time, compile_datetime[1], sizeof(time));
  
  *year = atoi(date + 7);
  *(date + 6) = 0;
  *day = atoi(date + 4);
  *(date + 3) = 0;
  for(i = 0;i < 12;++i){
    if(0 == strcmp(date, months[i])){
      *month = i + 1;
      break;
    }
  }
  
  *second = atoi(time + 6);
  *(time + 5) = 0;
  *minute = atoi(time + 3);
  *(time + 2) = 0;
  *hour = atoi(time);
}
