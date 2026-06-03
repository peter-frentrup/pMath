#include <pmath-util/version.h>

#include <pmath.h>

#include <string.h>

#include "version.g.c"


#define PMATH_VERSION_MAJOR  0
#define PMATH_VERSION_MINOR  10
#define PMATH_VERSION_PATCH  0

PMATH_API
void pmath_version_datetime(
  int *year,
  int *month,
  int *day,
  int *hour,
  int *minute,
  int *second
) {
//  pmath_debug_print("[git: %s]\n", GIT_COMMIT_SHA);
  
  // yyyy-MM-ddThh:mm:ss+xx:xx
  // yyyy-MM-ddThh:mm:ss-xx:xx
  // yyyy-MM-ddThh:mm:ssZ
  *year   = ((GIT_COMMIT_DATE_ISO)[ 0] - '0') * 1000 // y
          + ((GIT_COMMIT_DATE_ISO)[ 1] - '0') * 100  // y
          + ((GIT_COMMIT_DATE_ISO)[ 2] - '0') * 10   // y
          + ((GIT_COMMIT_DATE_ISO)[ 3] - '0');       // y
  //                                4                // -
  *month  = ((GIT_COMMIT_DATE_ISO)[ 5] - '0') * 10   // M
          + ((GIT_COMMIT_DATE_ISO)[ 6] - '0');       // M
  //                                7                // -
  *day    = ((GIT_COMMIT_DATE_ISO) [8] - '0') * 10   // d
          + ((GIT_COMMIT_DATE_ISO)[ 9] - '0');       // d
  //                               10                // T
  *hour   = ((GIT_COMMIT_DATE_ISO)[11] - '0') * 10   // h
          + ((GIT_COMMIT_DATE_ISO)[12] - '0');       // h
  //                               13                // :
  *minute = ((GIT_COMMIT_DATE_ISO)[14] - '0') * 10   // m
          + ((GIT_COMMIT_DATE_ISO)[15] - '0');       // m
  //                               16                // :
  *second = ((GIT_COMMIT_DATE_ISO)[17] - '0') * 10   // s
          + ((GIT_COMMIT_DATE_ISO)[18] - '0');       // s
}

PMATH_API
double pmath_version_number(void) {
  return PMATH_VERSION_MAJOR + 0.01 * PMATH_VERSION_MINOR;
}

PMATH_API
long pmath_version_number_part(int index) {
  switch(index) {
    case 1:
      return PMATH_VERSION_MAJOR;
      
    case 2:
      return PMATH_VERSION_MINOR;
      
    case 3:
      return PMATH_VERSION_PATCH;
    
    case 4:
      return GIT_COMMITS_MASTER + GIT_COMMITS_SINCE_MASTER;
  }
  
  return 0;
}
