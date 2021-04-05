#ifndef RICHMATH__UTIL__SELECTION_TRACKING_H__INCLUDED
#define RICHMATH__UTIL__SELECTION_TRACKING_H__INCLUDED

#include <util/array.h>
#include <util/hashtable.h>
#include <util/selections.h>

namespace richmath {
  class Section;
  
  bool toggle_edit_section(
    Section *section, 
    ArrayView<const LocationReference> old_locations,
    Hashtable<LocationReference, SelectionReference> &found_locations);
}

#endif // RICHMATH__UTIL__SELECTION_TRACKING_H__INCLUDED
