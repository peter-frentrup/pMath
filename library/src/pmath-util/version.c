#include <pmath-util/version.h>
#include <pmath-util/version-private.h>

#include <pmath.h>


PMATH_API
void pmath_version_datetime(
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
  
  memcpy(date, __DATE__, sizeof(date));
  memcpy(time, __TIME__, sizeof(time));
  
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

PMATH_API
double pmath_version_number(void){
  return _PMATH_VERSION_MAJOR + 0.01 * _PMATH_VERSION_MINOR;
}

PMATH_API
long pmath_version_number_part(int index){
  switch(index){
    case 1:
      return _PMATH_VERSION_MAJOR;
      
    case 2:
      return _PMATH_VERSION_MINOR;
    
    case 3:
      return _PMATH_VERSION_BUILD;
    
    case 4:
      return strtol(_PMATH_VERSION_SVN_REVISION, NULL, 10);
  }
  
  return 0;
}
