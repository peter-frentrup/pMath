#ifndef RICHMATH__GUI__EDIT_HELPER_H__INCLUDED
#define RICHMATH__GUI__EDIT_HELPER_H__INCLUDED

namespace richmath {
  class EditHelper {
    public:
      static bool insert_new_table_into_current_document();
      static bool insert_new_table_into_current_document(int num_rows, int num_cols);
  };
}

#endif // RICHMATH__GUI__EDIT_HELPER_H__INCLUDED
