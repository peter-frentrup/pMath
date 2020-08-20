#ifndef __PMATH_CPP_H__
#define __PMATH_CPP_H__

#include <limits>
#include <climits>

#include <pmath.h>

#if __cplusplus >= 201103L
#  define PMATH_CPP_USE_RVALUE_REF
#  define PMATH_CPP_SPECIALIZE_STD_HASH
#elif defined(_MSC_VER)
#  if _MSC_VER >= 1600 /* MSVC++ 2010 */
#    define PMATH_CPP_USE_RVALUE_REF
#    define PMATH_CPP_SPECIALIZE_STD_HASH
#  endif
#endif

#ifdef PMATH_CPP_USE_RVALUE_REF
#  include <utility>
#  define PMATH_CPP_MOVE(m)   std::move(m)
#else
#  define PMATH_CPP_MOVE(m)   m
#endif

/**\defgroup cpp_binding C++ Binding

   There exists a thin layer to easily use pMath with C++. This is usably
   prefreable over the C API because it handles reference counting/type checking
   automatically and leads to less "boilerplate code".

   To use it, simply <tt>\#include <pmath-cpp.h></tt>. The classes are in the
   \c pmath namespace.

   This namespace also containts numerous helper functions to easily construct
   expression trees. None of the classes and functions generate C++ exceptions,
   they are all fault tolerant (in contrast to most of the plain C API).

   @{
 */

/**\brief Provides the C++ binding.
 */
namespace pmath {
  class ReadableBinaryFile;
  class String;
  class WriteableBinaryFile;
  class WriteableTextFile;
  
  /**\ingroup cpp_binding
     \brief A wrapper for pmath_t and drived types.
  
     This class wraps a single pmath_t, so its sizeof(Expr) == sizeof(pmath_t).
   */
  class Expr {
    public:
      class ItemsEnum;
      class ReverseItemsEnum;
      
      class Iterator {
          friend class ItemsEnum;
        public:
          bool operator!=(const Iterator &other) const throw() {
            return _expr_ptr != other._expr_ptr || _index != other._index;
          }
          Expr operator*() const {
            return (*_expr_ptr)[_index];
          }
          const Iterator &operator++() {
            ++_index;
            return *this;
          }
          
        private:
          Iterator(const Expr *expr_ptr, const size_t index)
            : _expr_ptr(expr_ptr), 
              _index(index)
          {
          }
          
        private:
          const Expr *_expr_ptr;
          size_t      _index;
      };
      
      class ReverseIterator {
          friend class ReverseItemsEnum;
        public:
          bool operator!=(const ReverseIterator &other) const throw() {
            return _expr_ptr != other._expr_ptr || _index_plus_1 != other._index_plus_1;
          }
          Expr operator*() const {
            return (*_expr_ptr)[_index_plus_1 - 1];
          }
          const ReverseIterator &operator++() {
            --_index_plus_1;
            return *this;
          }
          
        private:
          ReverseIterator(const Expr *expr_ptr, const size_t index_plus_1)
            : _expr_ptr(expr_ptr), 
              _index_plus_1(index_plus_1)
          {
          }
          
        private:
          const Expr *_expr_ptr;
          size_t      _index_plus_1;
      };
      
      class ItemsEnum {
        public:
          ItemsEnum(const Expr *expr_ptr, size_t first_index, size_t last_index) 
            : _expr_ptr(expr_ptr),
              _first_index(first_index),
              _last_index(last_index < first_index ? first_index - 1 : last_index)
          {
          }
          
          Iterator begin() const throw() {
            return Iterator(_expr_ptr, _first_index);
          }
          
          Iterator end() const throw() {
            return Iterator(_expr_ptr, _last_index + 1);
          }
        
        private:
          const Expr *_expr_ptr;
          const size_t _first_index;
          const size_t _last_index;
      };
      
      class ReverseItemsEnum {
        public:
          ReverseItemsEnum(const Expr *expr_ptr, size_t smallest_index, size_t largest_index) 
            : _expr_ptr(expr_ptr),
              _smallest_index(smallest_index),
              _largest_index(largest_index < smallest_index ? smallest_index - 1 : largest_index)
          {
          }
          
          ReverseIterator begin() const throw() {
            return ReverseIterator(_expr_ptr, _largest_index + 1);
          }
          
          ReverseIterator end() const throw() {
            return ReverseIterator(_expr_ptr, _smallest_index);
          }
        
        private:
          const Expr *_expr_ptr;
          const size_t _smallest_index;
          const size_t _largest_index;
      };
      
    public:
      /**\brief Initialize with PMATH_NULL */
      Expr() throw()
        : _obj(PMATH_NULL)
      {
      }
      
      /**\brief Construct form a pmath_t, that will be freed automatically with the Expr. */
      explicit Expr(pmath_t obj) throw()
        : _obj(obj)
      {
      }
      
      /**\brief Copy an Expr, inrecementing the reference counter. */
      Expr(const Expr &src) throw()
        : _obj(pmath_ref(src._obj))
      {
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      Expr(Expr &&src) throw()
        : _obj(src._obj)
      {
        src._obj = PMATH_NULL;
      }
#endif
      
      /**\brief Construct from an int. */
      Expr(int i) throw()
        : _obj(pmath_build_value("i", i))
      {
      }
      
      /**\brief Construct from an int64_t. */
      Expr(int64_t i) throw()
        : _obj(pmath_integer_new_si64(i))
      {
      }
      
      /**\brief Construct from an size_t. */
      Expr(size_t i) throw()
        : _obj(pmath_integer_new_uiptr(i))
      {
      }
      
      /**\brief Construct from a double. May yield Infinity or Undefined (NaN) values */
      Expr(double f) throw()
        : _obj(pmath_build_value("f", f))
      {
      }
      
      /**\brief Destructor. Frees the wrapped object. */
      ~Expr() throw() {
        pmath_unref(_obj);
      }
      
      /**\brief Copy an Expr. Increments the new value's reference counter and frees the old one. */
      Expr &operator=(Expr other) throw() {
        swap(*this, other);
        return *this;
      }
      
      friend void swap(Expr &first, Expr &second) throw() {
        using std::swap;
        swap(first._obj, second._obj);
      }
      
      bool is_custom()       const throw() { return pmath_is_custom(_obj); }
      bool is_double()       const throw() { return pmath_is_double(_obj); }
      bool is_expr()         const throw() { return pmath_is_expr(_obj); }
      bool is_float()        const throw() { return pmath_is_float(_obj); }
      bool is_integer()      const throw() { return pmath_is_integer(_obj); }
      bool is_int32()        const throw() { return pmath_is_int32(_obj); }
      bool is_magic()        const throw() { return pmath_is_magic(_obj); }
      bool is_mpfloat()      const throw() { return pmath_is_mpfloat(_obj); }
      bool is_null()         const throw() { return pmath_is_null(_obj); }
      bool is_number()       const throw() { return pmath_is_number(_obj); }
      bool is_pointer()      const throw() { return pmath_is_pointer(_obj); }
      bool is_quotient()     const throw() { return pmath_is_quotient(_obj); }
      bool is_rational()     const throw() { return pmath_is_rational(_obj); }
      bool is_string()       const throw() { return pmath_is_string(_obj); }
      bool is_symbol()       const throw() { return pmath_is_symbol(_obj); }
      bool is_blob()         const throw() { return pmath_is_blob(_obj); }
      bool is_packed_array() const throw() { return pmath_is_packed_array(_obj); }
      
      bool is_pointer_of(pmath_type_t type) const throw() {
        return pmath_is_pointer_of(_obj, type);
      }
      
      bool is_evaluated() const throw() {
        return pmath_is_evaluated(_obj);
      }
      
      bool is_rule() const throw() {
        if(expr_length() != 2)
          return false;
          
        Expr head = (*this)[0];
        if(pmath_same(head._obj, PMATH_SYMBOL_RULE))
          return true;
          
        if(pmath_same(head._obj, PMATH_SYMBOL_RULEDELAYED))
          return true;
          
        return false;
      }

      bool is_list_of_rules() const throw() {
        return pmath_is_list_of_rules(_obj);
      }
      
      /**\brief Get a hash value. */
      unsigned int hash() const throw() {
        return pmath_hash(_obj);
      }
      
      /**\brief Check for identity. The pMath === operator. */
      bool operator==(const Expr &other) const throw() {
        return pmath_equals(_obj, other._obj);
      }
      
      /**\brief Check for non-identity. The pMath =!= operator. */
      bool operator!=(const Expr &other) const throw() {
        return !pmath_equals(_obj, other._obj);
      }
      
      /**\brief Compare with another Expr. See pmath_compare() */
      int compare(const Expr &other) const throw() {
        return pmath_compare(_obj, other._obj);
      }
      
      /**\brief Compare with another Expr. See pmath_compare() */
      bool operator<(const Expr &other) const throw() {
        return compare(other) < 0;
      }
      
      /**\brief Compare with another Expr. See pmath_compare() */
      bool operator<=(const Expr &other) const throw() {
        return compare(other) <= 0;
      }
      
      /**\brief Compare with another Expr. See pmath_compare() */
      bool operator>(const Expr &other) const throw() {
        return compare(other) > 0;
      }
      
      /**\brief Compare with another Expr. See pmath_compare() */
      bool operator>=(const Expr &other) const throw() {
        return compare(other) >= 0;
      }
      
      /**\brief Return the pmath_t and discard it. Caller must pmath_unref() it. */
      pmath_t release() throw() { pmath_t o = _obj; _obj = PMATH_NULL; return o; }
      
      /**\brief Get the pmath_t. Reference is held by the Expr object. */
      const pmath_t get() const throw() { return _obj; }
      
      /**\brief Check for not holding the null pointer. */
      bool is_valid() const throw() { return !is_null(); }
      
      /**\brief Check for not holding the null pointer. */
      explicit operator bool() const throw() { return is_valid(); }
      
      /**\brief Length of the expression or 0 on error. */
      size_t expr_length() const throw() {
        if(!is_expr())
          return 0;
        return pmath_expr_length(_obj);
      }
      
      /**\brief Get the i-th argument of the expression.
         \param i Index. May be > expr_length().
         \return The i-th argument if the object is a \ref pmath_expr_t.
         expr[0] is the head, expr[1] the first argument and expr[length()] the
         last argument.
       */
      Expr operator[](size_t i) const throw() {
        if(!is_expr())
          return Expr();
        return Expr(pmath_expr_get_item(_obj, i));
      }
      
      /**\brief Get the i-th argument of the expression.
         \param i Index. Values outside 0..expr_length() give PMATH_NULL.
         \return The i-th argument if the object is a \ref pmath_expr_t.
       */
      Expr operator[](int i) const throw() {
        if(i < 0)
          return Expr();
        if(!is_expr())
          return Expr();
        return Expr(pmath_expr_get_item(_obj, (size_t)i));
      }

      bool try_lookup(Expr key, Expr &result) const throw() {
        return pmath_rules_lookup(_obj, key.release(), &result._obj);
      }

      Expr lookup(Expr key, Expr default_value) const throw() {
        pmath_rules_lookup(_obj, key.release(), &default_value._obj);
        return default_value;
      }
      
      /**\brief Enumerate all items [1..length()] with C++ range based for loops
       */
      ItemsEnum items() const throw() {
        return items((size_t)1, expr_length());
      }
      
      /**\brief Enumerate some items with C++ range based for loops.
         \param first Index of the first item, default is 1.
         \param last  Index of the last item (inclusive), default is expr_length().
       */
      ItemsEnum items(size_t first, size_t last) const throw() {
        return ItemsEnum(this, first, last);
      }
      
      /**\brief Enumerate all items in reverse order with C++ range based for loops
       */
      ReverseItemsEnum items_reverse() const throw() {
        return items_reverse((size_t)1, expr_length());
      }
      
      /**\brief Enumerate some items in reverse with C++ range based for loops.
         \param smallest Index of the first item (returned last), default is 1.
         \param largest  Index of the last item (returned first), default is expr_length().
       */
      ReverseItemsEnum items_reverse(size_t smallest, size_t largest) const throw() {
        return ReverseItemsEnum(this, smallest, largest);
      }
      
      void set_lookup(Expr key, Expr rhs) throw() {
        struct Modifier {
          Expr rhs;

          static pmath_bool_t callback(pmath_t *old_rhs, pmath_bool_t is_simple_rule, void *self) throw() {
            return ((Modifier*)self)->callback(old_rhs, is_simple_rule);
          }

          pmath_bool_t callback(pmath_t *old_rhs, pmath_bool_t is_simple_rule) throw() {
            using std::swap;
            swap(*old_rhs, rhs._obj);
            return is_simple_rule;
          }
        } modifier { PMATH_CPP_MOVE(rhs) };
        _obj = pmath_rules_modify(_obj, key.release(), Modifier::callback, &modifier);
      }
      
      /**\brief Change the i-th argument of an expression
         \param i Index. May be > expr_length().
         \param e The new element.
       */
      void set(size_t i, Expr e) throw() {
        if(is_expr())
          _obj = pmath_expr_set_item(_obj, i, e.release());
      }
      
      /**\brief Change the i-th argument of an expression
         \param i Index. Values outside 0..expr_length() are ignored.
         \param e The new element.
       */
      void set(int i, Expr e) throw() {
        if(i < 0)
          return;
        if(is_expr())
          _obj = pmath_expr_set_item(_obj, (size_t)i, e.release());
      }
      
      /**\brief Change the (i,j)-th argument of a matrix
         \param i The matrix row.
         \param j The matrix column.
         \param e The new element.
       */
      void set(size_t i, size_t j, Expr e) throw() {
        if(!is_expr())
          return;
          
        pmath_t item = pmath_expr_extract_item(_obj, i);
        if(pmath_is_expr(item)) {
          item = pmath_expr_set_item(item, j, e.release());
          _obj = pmath_expr_set_item(_obj, i, item);
        }
        else {
          _obj = pmath_expr_set_item(_obj, i, item);
        }
      }
      
      /**\brief Change the (i,j)-th argument of a matrix
         \param i The matrix row.
         \param j The matrix column.
         \param e The new element.
       */
      void set(int i, int j, Expr e) throw() {
        if(i < 0 || j < 0 || !is_expr())
          return;
          
        pmath_t item = pmath_expr_extract_item(_obj, (size_t)i);
        if(pmath_is_expr(item)) {
          item = pmath_expr_set_item(item, (size_t)j, e.release());
          _obj = pmath_expr_set_item(_obj, (size_t)i, item);
        }
        else {
          _obj = pmath_expr_set_item(_obj, (size_t)i, item);
        }
      }
      
      /**\brief Append an item to an expression.
         \param e The new element.
        */
      void append(Expr e) throw() {
        if(is_expr())
          _obj = pmath_expr_append(_obj, 1, e.release());
      }
      
      void expr_remove_all(Expr rem) throw() {
        if(is_expr())
          _obj = pmath_expr_remove_all(_obj, rem.get());
      }
      
      /**\brief Sort the expression
        */
      void sort() throw() {
        if(is_expr())
          _obj = pmath_expr_sort(_obj);
      }
      
      /**\brief Convert to a double.
         \param def Optional default value.
         \return the double value if the object is a numeric object and \a def
                 otherwise.
       */
      double to_double(double def = 0.0) const throw() {
        if(is_number())
          return pmath_number_get_d(_obj);
          
        pmath_t approx = pmath_set_precision(
                           pmath_ref(_obj),
                           -::std::numeric_limits<double>::infinity());
                           
        if(pmath_is_number(approx))
          def = pmath_number_get_d(approx);
          
        pmath_unref(approx);
        return def;
      }
      
      /**\brief Convert to a string. The pMath ToString function.
         \param options Optional formating options.
         \return The String representation.
       */
      String to_string(pmath_write_options_t options = 0) const throw();
      
      /**\brief Write to a file/text stream.
         \param file The text file object. It must be writeable.
         \param options Optional formating options.
       */
      void write_to_file(WriteableTextFile file, pmath_write_options_t options = 0) const throw();
      
      /**\brief Serialize to a binary file/stream.
         \param file The binary file/stream. It must be writeable.
         \param flags Serialization options.
         \return An error number.
       */
      pmath_serialize_error_t serialize(WriteableBinaryFile file, int flags = 0) const throw();
      
      /**\brief Deserialize an Expr from a binary file/stream.
         \param file The binary file/stream. It must be writeable.
         \param error An error number is stored here. May be NULL if not needed.
         \return The deserialized expression.
       */
      static Expr deserialize(ReadableBinaryFile file, pmath_serialize_error_t *error) throw();
      
    protected:
      /**\private */
      pmath_t _obj;
      
    protected:
      static void write_to_string(
        void           *user,
        const uint16_t *data,
        int             len
      ) throw() {
        *(pmath_string_t *)user = pmath_string_insert_ucs2(
                                    *(pmath_string_t *)user,
                                    INT_MAX,
                                    data,
                                    len);
      }
  };
  
  /**\brief check for identity. The pMath === operator.
     \memberof pmath::Expr
   */
  inline bool operator==(pmath_t o1, const Expr &o2) throw() {
    return pmath_equals(o1, o2.get());
  }
  
  /**\brief check for non-identity. The pMath =!= operator.
     \memberof pmath::Expr
   */
  inline bool operator!=(pmath_t o1, const Expr &o2) throw() {
    return !pmath_equals(o1, o2.get());
  }
  
  /**\memberof pmath::Expr
   */
  inline bool operator==(const Expr &o1, pmath_t o2) throw() {
    return pmath_equals(o1.get(), o2);
  }
  
  /**\memberof pmath::Expr
   */
  inline bool operator!=(const Expr &o1, pmath_t o2) throw() {
    return !pmath_equals(o1.get(), o2);
  }
  
  
  /**\ingroup cpp_binding
     \brief A wrapper for pmath_string_t.
  
     This class provides some string utility functions in addition to Expr.
   */
  class String: public Expr {
    public:
      String() throw()
        : Expr()
      {
      }
      
      /**\brief Construct form a pmath_string_t, stealing the reference. */
      explicit String(pmath_string_t _str) throw()
        : Expr(pmath_is_string(_str) ? pmath_ref(_str) : PMATH_NULL)
      {
        pmath_unref(_str);
      }
      
      String(const Expr &src) throw()
        : Expr(src.is_string() ? pmath_ref(src.get()) : PMATH_NULL)
      {
      }
      
      String(const String &src) throw()
        : Expr(pmath_ref(src._obj))
      {
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      explicit String(Expr &&src) throw()
        : Expr(src.is_string() ? std::move(src) : Expr())
      {
      }
      
      String(String  &&src) throw()
        : Expr(std::move(src))
      {
      }
#endif
      
      /**\brief Construct from Latin-1 encoded C string. */
      String(const char *latin1, int len = -1) throw()
        : Expr(latin1 ? pmath_string_insert_latin1(PMATH_NULL, 0, latin1, len) : PMATH_NULL)
      {
      }
      
      /**\brief Construct from UCS-2/UTF-16 encoded string. */
      static String FromUcs2(const uint16_t *ucs2, int len = -1) throw() {
        return String(pmath_string_insert_ucs2(PMATH_NULL, 0, ucs2, len));
      }
      
      /**\brief Construct from a single unicode character. */
      static String FromChar(unsigned int unicode) throw() {
        uint16_t u16[2];
        if(unicode <= 0xFFFF) {
          u16[0] = (uint16_t)unicode;
          return String(pmath_string_insert_ucs2(PMATH_NULL, 0, u16, 1));
        }
        
        if(  unicode > 0x10FFFF          ||
             (unicode & 0xFC00) == 0xD800 ||
             (unicode & 0xFC00) == 0xDC00)
        {
          return String();
        }
        
        unicode -= 0x10000;
        u16[0] = 0xD800 | (unicode >> 10);
        u16[1] = 0xDC00 | (unicode & 0x3FF);
        
        return String(pmath_string_insert_ucs2(PMATH_NULL, 0, u16, 2));
      }
      
      /**\brief Construct from UTF-8 encoded C string. */
      static String FromUtf8(const char *utf8, int len = -1) throw() {
        return String(pmath_string_from_utf8(utf8, len));
      }
      
      String &operator=(const String &src) throw() {
        Expr::operator=(src);
        return *this;
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      String &operator=(String && src) throw() {
        Expr::operator=(std::move(src));
        return *this;
      }
#endif
      
      /**\brief Append a string. */
      String &operator+=(const String &src) throw() {
        _obj = pmath_string_concat(_obj, (pmath_string_t)pmath_ref(src.get()));
        return *this;
      }
#ifdef PMATH_CPP_USE_RVALUE_REF
      String &operator+=(String &&src) throw() {
        _obj = pmath_string_concat(_obj, (pmath_string_t)src.release());
        return *this;
      }
#endif
      
      /**\brief Append a C string. */
      String &operator+=(const char *latin1) throw() {
        _obj = pmath_string_insert_latin1(_obj, INT_MAX, latin1, -1);
        return *this;
      }
      
      /**\brief Append a UTF-16-string. */
      String &operator+=(const uint16_t *ucs2) throw() {
        _obj = pmath_string_insert_ucs2(_obj, INT_MAX, ucs2, -1);
        return *this;
      }
      
      /**\brief Append a single unicode character. */
      String &operator+=(uint16_t ch) throw() {
        _obj = pmath_string_insert_ucs2(_obj, INT_MAX, &ch, 1);
        return *this;
      }
      
      /**\brief Concatenate two strings. */
      String operator+(const String &other) const throw() {
        return String(pmath_string_concat(
                        (pmath_string_t)pmath_ref(_obj),
                        (pmath_string_t)pmath_ref(other.get())));
      }
#ifdef PMATH_CPP_USE_RVALUE_REF
      String operator+(String &&other) const throw() {
        return String(pmath_string_concat(
                        (pmath_string_t)pmath_ref(_obj),
                        (pmath_string_t)other.release()));
      }
#endif
      
      String operator+(const char *latin1) const throw() {
        return String(pmath_string_insert_latin1(
                        (pmath_string_t)pmath_ref(_obj),
                        INT_MAX,
                        latin1,
                        -1));
      }
      
      String operator+(const uint16_t *ucs2) const throw() {
        return String(pmath_string_insert_ucs2(
                        (pmath_string_t)pmath_ref(_obj),
                        INT_MAX,
                        ucs2,
                        -1));
      }
      
      /**\brief Concatenate a String and a single unicode character. */
      String operator+(uint16_t ch) const throw() {
        return String(pmath_string_insert_ucs2(
                        (pmath_string_t)pmath_ref(_obj),
                        INT_MAX,
                        &ch,
                        1));
      }
      
      /**\brief Get string part. */
      String part(int start, int length = -1) const throw() {
        return String(pmath_string_part(
                        (pmath_string_t)pmath_ref(_obj), start, length));
      }
      
      /**\brief Check for equality with a C string (Latin-1 encoded). */
      bool equals(const char *latin1) const throw() {
        return pmath_string_equals_latin1((pmath_string_t)_obj, latin1);
      }
      
      /**\brief Check for prefix equality. */
      bool starts_with(const String &s) const throw() {
        return starts_with_buffer(s.buffer(), s.length());
      }
      
      bool starts_with(const char *latin1, int len = -1) const throw() {
        return starts_with_buffer((const unsigned char*)latin1, len);
      }
      
      bool starts_with(const uint16_t *ucs2, int len = -1) const throw() {
        return starts_with_buffer(ucs2, len);
      }
      
      /**\brief Insert a substring. Changes the object itself. */
      void insert(int pos, const String &other) throw() {
        _obj = pmath_string_insert(_obj, pos, pmath_ref(other.get()));
      }
#ifdef PMATH_CPP_USE_RVALUE_REF
      void insert(int pos, String &&other) throw() {
        _obj = pmath_string_insert(_obj, pos, other.release());
      }
#endif
      
      void insert(int pos, const char *latin1, int len = -1) throw() {
        _obj = pmath_string_insert_latin1(_obj, pos, latin1, len);
      }
      
      void insert(int pos, const uint16_t *ucs2, int len = -1) throw() {
        _obj = pmath_string_insert_ucs2(_obj, pos, ucs2, len);
      }
      
      /**\brief Remove a substring. Changes the object itself. */
      void remove(int start, int length) throw() {
        pmath_string_t prefix = pmath_string_part(
                                  (pmath_string_t)pmath_ref(_obj), 0, start);
                                  
        pmath_string_t postfix = pmath_string_part(
                                   (pmath_string_t)pmath_ref(_obj), start + length,
                                   pmath_string_length(_obj) - start - length);
                                   
        pmath_unref(_obj);
        _obj = pmath_string_concat(prefix, postfix);
      }
      
      /**\brief Trim leading and trailing whitespace. */
      const String trim() const throw() {
        int end = length() - 1;
        int start = 0;
        const uint16_t *buf = buffer();
        
        while(start <= end && buf[start] <= ' ')
          ++start;
          
        while(start < end && buf[end] <= ' ')
          --end;
          
        return part(start, end + 1 - start);
      }
      
      /**\brief Get the string length. */
      int length() const throw() {
        return pmath_string_length(_obj);
      }
      
      /**\brief Get the UCS-2/UTF-16 const string buffer. This is not zero-terminated */
      const uint16_t *buffer() const throw() {
        return pmath_string_buffer(const_cast<pmath_string_t *>(&_obj));
      }
      
      /**\brief Get a single character or U+0000 on error. */
      uint16_t operator[](int i) const throw() {
        if(i < 0 || i >= length())
          return 0;
        return buffer()[i];
      }
      
      /**\brief Get the underlying pmath_string_t. It remains owned by this object. */
      const pmath_string_t get_as_string() const throw() { return (pmath_string_t)_obj; }
      
    private:
      template<class UnsignedCharType>
      bool starts_with_buffer(const UnsignedCharType *prefix, int len = -1) const throw() {
        if(len < 0) {
          const UnsignedCharType *tmp = prefix;
          len = 0;
          while(*tmp++)
            ++len;
        }
        
        if(len > length())
          return false;
          
        const uint16_t *buf = buffer();
        while(len-- > 0)
          if(*buf++ != *(UnsignedCharType *)prefix++)
            return false;
            
        return true;
      }
      
    private:
      String &operator=(const Expr &src) { // use explicit cast instead
        return *this;
      }
#ifdef PMATH_CPP_USE_RVALUE_REF
      String &operator=(Expr && src) { // use explicit cast instead
        return *this;
      }
#endif
  };
  
  /**\ingroup cpp_binding
     \brief Utility class for emitting and gathering expressions/building lists.
  
     Gathering begins with the construction of the object and ends with a call
     to end() or the object destruction. This removes the burden of calling
     pmath_gather_end() for every pmath_gather_begin().
    */
  class Gather {
    public:
      /**\brief The constructor. Starts gathering. */
      Gather() throw()
        : ended(false)
      {
        pmath_gather_begin(PMATH_NULL);
      }
      
      explicit Gather(Expr pattern) throw()
        : ended(false)
      {
        pmath_gather_begin(pattern.release());
      }
      
      ~Gather() throw() {
        end();
      }
      
      /**\brief end gathering. Calling end() multiple times returns PMATH_NULL. */
      Expr end() throw() {
        if(ended)
          return Expr();
        ended = true;
        return Expr(pmath_gather_end());
      }
      
      /**\brief Emit a value to be gathered. */
      static void emit(Expr e) throw() {
        pmath_emit(e.release(), PMATH_NULL);
      }
      
      /**\brief Emit a value to be gathered. */
      static void emit(Expr e, Expr tag) throw() {
        pmath_emit(e.release(), tag.release());
      }
      
    protected:
      bool ended;
      
    private:
      Gather(const Gather &src) {
      }
      Gather &operator=(const Gather &src) {
        return *this;
      }
  };
  
  
  
  
  inline Expr Number(double d) {
    if((double)((int)d) == d)
      return Expr((int)d);
      
    return Expr(d);
  }
  inline Expr Complex(Expr re, Expr im)    { return Expr(pmath_build_value("Coo", re.release(), im.release())); }
  inline Expr Imaginary(Expr im)           { return Complex(0, PMATH_CPP_MOVE(im)); }
  inline Expr Rational(Expr num, Expr den) { return Expr(pmath_build_value("Qoo", num.release(), den.release())); }
  
  inline Expr Ref(pmath_t o)           { return Expr(pmath_ref(o)); }
  inline Expr Symbol(pmath_symbol_t h) { return Ref(h); }
  inline Expr SymbolPi()               { return Symbol(PMATH_SYMBOL_PI); }
  
  inline Expr MakeList(size_t len) { return Expr(pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), len)); }
  
  inline Expr Call(Expr h) {
    return Expr(pmath_expr_new(h.release(), 0));
  }
  inline Expr Call(Expr h, Expr x1) {
    return Expr(pmath_expr_new_extended(h.release(), 1, x1.release()));
  }
  inline Expr Call(Expr h, Expr x1, Expr x2) {
    return Expr(pmath_expr_new_extended(h.release(), 2, x1.release(), x2.release()));
  }
  inline Expr Call(Expr h, Expr x1, Expr x2, Expr x3) {
    return Expr(pmath_expr_new_extended(h.release(), 3, x1.release(), x2.release(), x3.release()));
  }
  inline Expr Call(Expr h, Expr x1, Expr x2, Expr x3, Expr x4) {
    return Expr(pmath_expr_new_extended(h.release(), 4, x1.release(), x2.release(), x3.release(), x4.release()));
  }
  inline Expr Call(Expr h, Expr x1, Expr x2, Expr x3, Expr x4, Expr x5) {
    return Expr(pmath_expr_new_extended(h.release(), 5, x1.release(), x2.release(), x3.release(), x4.release(), x5.release()));
  }
  inline Expr Call(Expr h, Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6) {
    return Expr(pmath_expr_new_extended(h.release(), 6, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release()));
  }
  inline Expr Call(Expr h, Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7) {
    return Expr(pmath_expr_new_extended(h.release(), 7, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release(), x7.release()));
  }
  inline Expr Call(Expr h, Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7, Expr x8) {
    return Expr(pmath_expr_new_extended(h.release(), 8, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release(), x7.release(), x8.release()));
  }
  inline Expr Call(Expr h, Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7, Expr x8, Expr x9) {
    return Expr(pmath_expr_new_extended(h.release(), 9, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release(), x7.release(), x8.release(), x9.release()));
  }
  
  inline Expr List() {
    return Call(Symbol(PMATH_SYMBOL_LIST));
  }
  inline Expr List(Expr x1) {
    return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 1, x1.release()));
  }
  inline Expr List(Expr x1, Expr x2) {
    return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 2, x1.release(), x2.release()));
  }
  inline Expr List(Expr x1, Expr x2, Expr x3) {
    return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 3, x1.release(), x2.release(), x3.release()));
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4) {
    return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 4, x1.release(), x2.release(), x3.release(), x4.release()));
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5) {
    return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 5, x1.release(), x2.release(), x3.release(), x4.release(), x5.release()));
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6) {
    return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 6, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release()));
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7) {
    return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 7, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release(), x7.release()));
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7, Expr x8) {
    return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 8, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release(), x7.release(), x8.release()));
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7, Expr x8, Expr x9) {
    return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 9, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release(), x7.release(), x8.release(), x9.release()));
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7, Expr x8, Expr x9, Expr x10) {
    return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 10, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release(), x7.release(), x8.release(), x9.release(), x10.release()));
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7, Expr x8, Expr x9, Expr x10, Expr x11) {
    return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 11, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release(), x7.release(), x8.release(), x9.release(), x10.release(), x11.release()));
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7, Expr x8, Expr x9, Expr x10, Expr x11, Expr x12) {
    return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 12, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release(), x7.release(), x8.release(), x9.release(), x10.release(), x11.release(), x12.release()));
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7, Expr x8, Expr x9, Expr x10, Expr x11, Expr x12, Expr x13) {
    return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 13, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release(), x7.release(), x8.release(), x9.release(), x10.release(), x11.release(), x12.release(), x13.release()));
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7, Expr x8, Expr x9, Expr x10, Expr x11, Expr x12, Expr x13, Expr x14) {
    return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 14, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release(), x7.release(), x8.release(), x9.release(), x10.release(), x11.release(), x12.release(), x13.release(), x14.release()));
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7, Expr x8, Expr x9, Expr x10, Expr x11, Expr x12, Expr x13, Expr x14, Expr x15) {
    return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_LIST), 15, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release(), x7.release(), x8.release(), x9.release(), x10.release(), x11.release(), x12.release(), x13.release(), x14.release(), x15.release()));
  }
  
  inline Expr Rule(       Expr l, Expr r) { return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_RULE),        2, l.release(), r.release())); }
  inline Expr RuleDelayed(Expr l, Expr r) { return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_RULEDELAYED), 2, l.release(), r.release())); }
  
  inline Expr Power(Expr x, Expr y) { return Expr(pmath_expr_new_extended(pmath_ref(PMATH_SYMBOL_POWER), 2, x.release(), y.release())); }

  inline Expr Sqrt(Expr x) { return Power(PMATH_CPP_MOVE(x), Rational(1, 2)); }
  inline Expr Inv(Expr x) {  return Power(PMATH_CPP_MOVE(x), -1); }
  
  inline Expr Exp(Expr x) { return Power(Symbol(PMATH_SYMBOL_E), PMATH_CPP_MOVE(x)); }
  
  inline Expr Log(Expr x) {         return Call(Symbol(PMATH_SYMBOL_LOG), PMATH_CPP_MOVE(x)); }
  inline Expr Log(Expr b, Expr x) { return Call(Symbol(PMATH_SYMBOL_LOG), PMATH_CPP_MOVE(b), PMATH_CPP_MOVE(x)); }

  inline Expr Sin(   Expr x) { return Call(Symbol(PMATH_SYMBOL_SIN), PMATH_CPP_MOVE(x)); }
  inline Expr Cos(   Expr x) { return Call(Symbol(PMATH_SYMBOL_COS), PMATH_CPP_MOVE(x)); }
  inline Expr Tan(   Expr x) { return Call(Symbol(PMATH_SYMBOL_TAN), PMATH_CPP_MOVE(x)); }
  inline Expr ArcSin(Expr x) { return Call(Symbol(PMATH_SYMBOL_ARCSIN), PMATH_CPP_MOVE(x)); }
  inline Expr ArcCos(Expr x) { return Call(Symbol(PMATH_SYMBOL_ARCCOS), PMATH_CPP_MOVE(x)); }
  inline Expr ArcTan(Expr x) { return Call(Symbol(PMATH_SYMBOL_ARCTAN), PMATH_CPP_MOVE(x)); }

  inline Expr Times(Expr x1, Expr x2) {                   return Call(Symbol(PMATH_SYMBOL_TIMES), PMATH_CPP_MOVE(x1), PMATH_CPP_MOVE(x2)); }
  inline Expr Times(Expr x1, Expr x2, Expr x3) {          return Call(Symbol(PMATH_SYMBOL_TIMES), PMATH_CPP_MOVE(x1), PMATH_CPP_MOVE(x2), PMATH_CPP_MOVE(x3)); }
  inline Expr Times(Expr x1, Expr x2, Expr x3, Expr x4) { return Call(Symbol(PMATH_SYMBOL_TIMES), PMATH_CPP_MOVE(x1), PMATH_CPP_MOVE(x2), PMATH_CPP_MOVE(x3), PMATH_CPP_MOVE(x4)); }

  inline Expr Divide(Expr x, Expr y) { return Times(PMATH_CPP_MOVE(x), Inv(PMATH_CPP_MOVE(y))); }

  inline Expr Plus(Expr x1, Expr x2) {                   return Call(Symbol(PMATH_SYMBOL_PLUS), PMATH_CPP_MOVE(x1), PMATH_CPP_MOVE(x2)); }
  inline Expr Plus(Expr x1, Expr x2, Expr x3) {          return Call(Symbol(PMATH_SYMBOL_PLUS), PMATH_CPP_MOVE(x1), PMATH_CPP_MOVE(x2), PMATH_CPP_MOVE(x3)); }
  inline Expr Plus(Expr x1, Expr x2, Expr x3, Expr x4) { return Call(Symbol(PMATH_SYMBOL_PLUS), PMATH_CPP_MOVE(x1), PMATH_CPP_MOVE(x2), PMATH_CPP_MOVE(x3), PMATH_CPP_MOVE(x4)); }

  inline Expr Minus(Expr x) {         return Times(-1, PMATH_CPP_MOVE(x)); }
  inline Expr Minus(Expr x, Expr y) { return Plus(PMATH_CPP_MOVE(x), Minus(PMATH_CPP_MOVE(y))); }
  
  inline Expr Abs( Expr x) { return Call(Symbol(PMATH_SYMBOL_ABS),  PMATH_CPP_MOVE(x)); }
  inline Expr Arg( Expr x) { return Call(Symbol(PMATH_SYMBOL_ARG),  PMATH_CPP_MOVE(x)); }
  inline Expr Sign(Expr x) { return Call(Symbol(PMATH_SYMBOL_SIGN), PMATH_CPP_MOVE(x)); }
  inline Expr Re(  Expr x) { return Call(Symbol(PMATH_SYMBOL_RE),   PMATH_CPP_MOVE(x)); }
  inline Expr Im(  Expr x) { return Call(Symbol(PMATH_SYMBOL_IM),   PMATH_CPP_MOVE(x)); }
  
  inline Expr Ceiling(Expr x) {         return Call(Symbol(PMATH_SYMBOL_CEILING), PMATH_CPP_MOVE(x)); }
  inline Expr Ceiling(Expr x, Expr a) { return Call(Symbol(PMATH_SYMBOL_CEILING), PMATH_CPP_MOVE(x), PMATH_CPP_MOVE(a)); }
  inline Expr Floor(  Expr x) {         return Call(Symbol(PMATH_SYMBOL_FLOOR),   PMATH_CPP_MOVE(x)); }
  inline Expr Floor(  Expr x, Expr a) { return Call(Symbol(PMATH_SYMBOL_FLOOR),   PMATH_CPP_MOVE(x), PMATH_CPP_MOVE(a)); }
  inline Expr Round(  Expr x) {         return Call(Symbol(PMATH_SYMBOL_ROUND),   PMATH_CPP_MOVE(x)); }
  inline Expr Round(  Expr x, Expr a) { return Call(Symbol(PMATH_SYMBOL_ROUND),   PMATH_CPP_MOVE(x), PMATH_CPP_MOVE(a)); }
  
  inline Expr Quotient(Expr m, Expr n) {         return Call(Symbol(PMATH_SYMBOL_QUOTIENT), PMATH_CPP_MOVE(m), PMATH_CPP_MOVE(n)); }
  inline Expr Quotient(Expr m, Expr n, Expr d) { return Call(Symbol(PMATH_SYMBOL_QUOTIENT), PMATH_CPP_MOVE(m), PMATH_CPP_MOVE(n), PMATH_CPP_MOVE(d)); }
  inline Expr Mod(     Expr m, Expr n) {         return Call(Symbol(PMATH_SYMBOL_MOD),      PMATH_CPP_MOVE(m), PMATH_CPP_MOVE(n)); }
  inline Expr Mod(     Expr m, Expr n, Expr d) { return Call(Symbol(PMATH_SYMBOL_MOD),      PMATH_CPP_MOVE(m), PMATH_CPP_MOVE(n), PMATH_CPP_MOVE(d)); }
  
  inline Expr Evaluate(Expr x) { return Expr(pmath_evaluate(x.release())); }
  
  inline Expr ParseArgs(const char *code, Expr arglist) {
    return Expr(pmath_parse_string_args(
                  code,
                  "o",
                  arglist.release()));
  }
  inline Expr Parse(String code) { return Expr(pmath_parse_string(code.release())); }
//  inline Expr Parse(const char *code) { return Expr(pmath_parse_string(PMATH_C_STRING(code))); }
  inline Expr Parse(const char *code, Expr x1) {
    return ParseArgs(code, List(PMATH_CPP_MOVE(x1)));
  }
  inline Expr Parse(const char *code, Expr x1, Expr x2) {
    return ParseArgs(code, List(PMATH_CPP_MOVE(x1), PMATH_CPP_MOVE(x2)));
  }
  inline Expr Parse(const char *code, Expr x1, Expr x2, Expr x3) {
    return ParseArgs(code, List(PMATH_CPP_MOVE(x1), PMATH_CPP_MOVE(x2), PMATH_CPP_MOVE(x3)));
  }
  inline Expr Parse(const char *code, Expr x1, Expr x2, Expr x3, Expr x4) {
    return ParseArgs(code, List(PMATH_CPP_MOVE(x1), PMATH_CPP_MOVE(x2), PMATH_CPP_MOVE(x3), PMATH_CPP_MOVE(x4)));
  }
  
  /**\ingroup cpp_binding
     \brief A wrapper for pMath file objects (data streams).
  
     This class provides some stream utility functions in addition to Expr.
     Note that a pMath file does not have to correspond to any operating system
     file object.
   */
  class File: public Expr {
    public:
      File() throw()
        : Expr()
      {
      }
      
      explicit File(pmath_t file_object) throw()
        : Expr(pmath_file_test(file_object, 0) ? pmath_ref(file_object) : PMATH_NULL)
      {
        pmath_unref(file_object);
      }
      
      explicit File(const Expr &file_object) throw()
        : Expr(pmath_file_test(file_object.get(), 0) ? file_object : Expr())
      {
      }
      
      File(const File &src) throw()
        : Expr(src)
      {
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      explicit File(Expr &&file_object) throw()
        : Expr(pmath_file_test(file_object.get(), 0) ? std::move(file_object) : Expr())
      {
      }
      
      File(File &&src) throw()
        : Expr(std::move(src))
      {
      }
#endif
      
      File &operator=(const File &src) {
        Expr::operator=(src);
        return *this;
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      File &operator=(File &&src) {
        Expr::operator=(std::move(src));
        return *this;
      }
#endif
      
      /**\brief Test for file properties/capabilities
         \param properties 0 or one or more of the PMATH_FILE_PROP_XXX constants.
         \return Whether the file has all the specified capabilities.
       */
      bool has_capabilities(int properties) const throw() { return pmath_file_test(_obj, properties); }
      
      bool is_file()      const throw() { return has_capabilities(0); }
      bool is_readable()  const throw() { return has_capabilities(PMATH_FILE_PROP_READ); }
      bool is_writeable() const throw() { return has_capabilities(PMATH_FILE_PROP_WRITE); }
      bool is_binary()    const throw() { return has_capabilities(PMATH_FILE_PROP_BINARY); }
      bool is_text()      const throw() { return has_capabilities(PMATH_FILE_PROP_TEXT); }
      
      pmath_files_status_t status() const throw() { return pmath_file_status(_obj); }
      
      /**\brief Flush the output buffer of a writeable file.
       */
      void flush() throw() {
      }
      
      /**\brief Closes a file immediatly instead of letting the garbage collector close it later.
       */
      void close() throw() {
        pmath_file_close(release());
      }
      
    private:
      File &operator=(const Expr &src) { // use explicit cast instead
        return *this;
      }
      
  };
  
  /**\ingroup cpp_binding
     \brief A wrapper for pMath binary file objects (byte data streams).
   */
  class BinaryFile: public File {
    public:
      BinaryFile()
        : File()
      {
      }
      
      explicit BinaryFile(pmath_t file_object) throw()
        : File(pmath_file_test(file_object, PMATH_FILE_PROP_BINARY) ? pmath_ref(file_object) : PMATH_NULL)
      {
        pmath_unref(file_object);
      }
      
      explicit BinaryFile(const Expr &file_object) throw()
        : File(File(file_object).is_binary() ? file_object : Expr())
      {
      }
      
      BinaryFile(const BinaryFile &src) throw()
        : File(src)
      {
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      explicit BinaryFile(Expr &&file_object) throw()
        : File(File(file_object).is_binary() ? std::move(file_object) : Expr())
      {
      }
      
      BinaryFile(BinaryFile &&src) throw()
        : File(std::move(src))
      {
      }
#endif
      
      BinaryFile &operator=(const BinaryFile &src) {
        File::operator=(src);
        return *this;
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      BinaryFile &operator=(BinaryFile &&src) {
        File::operator=(std::move(src));
        return *this;
      }
#endif
      
      /**\brief Create a memory buffer for as a double ended queue.
         \return The binary file. Can be used as ReadableBinaryFile and as
                 WriteableBinaryFile
       */
      static BinaryFile create_buffer(size_t mincapacity) throw() {
        return BinaryFile(pmath_file_create_binary_buffer(mincapacity));
      }
      
      /**\brief Get the current buffer size in bytes. */
      size_t get_buffer_size() const throw() {
        return pmath_file_binary_buffer_size(_obj);
      }
      
      /**\brief See file_set_binbuffer(). */
      bool set_buffer_size(size_t size) throw() {
        return pmath_file_set_binbuffer(_obj, size);
      }
  };
  
  
  /**\ingroup cpp_binding
     \brief A wrapper for readable pMath binary file objects (byte data streams).
   */
  class ReadableBinaryFile: public BinaryFile {
    public:
      ReadableBinaryFile()
        : BinaryFile()
      {
      }
      
      explicit ReadableBinaryFile(pmath_t file_object) throw()
        : BinaryFile(pmath_file_test(file_object, PMATH_FILE_PROP_READ) ? pmath_ref(file_object) : PMATH_NULL)
      {
        pmath_unref(file_object);
      }
      
      explicit ReadableBinaryFile(const Expr &file_object) throw()
        : BinaryFile(File(file_object).is_readable() ? file_object : Expr())
      {
      }
      
      ReadableBinaryFile(const ReadableBinaryFile &src) throw()
        : BinaryFile(src)
      {
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      explicit ReadableBinaryFile(Expr &&file_object) throw()
        : BinaryFile(File(file_object).is_readable() ? std::move(file_object) : Expr())
      {
      }
      
      ReadableBinaryFile(ReadableBinaryFile &&src) throw()
        : BinaryFile(std::move(src))
      {
      }
#endif
      
      ReadableBinaryFile &operator=(const ReadableBinaryFile &src) {
        BinaryFile::operator=(src);
        return *this;
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      ReadableBinaryFile &operator=(ReadableBinaryFile &&src) {
        BinaryFile::operator=(std::move(src));
        return *this;
      }
#endif
      
      /**\brief Create binary file object whose content is uncompressed from another binary file. */
      static ReadableBinaryFile create_decompressor(ReadableBinaryFile srcfile, struct pmath_decompressor_settings_t *settings = nullptr) throw() {
        return ReadableBinaryFile(pmath_file_create_decompressor(srcfile.release(), settings));
      }
      
      /**\brief Read some bytes from the file. See pmath_file_read(). */
      size_t read(void *buffer, size_t buffer_size, bool preserve_internal_buffer = false) throw() {
        return pmath_file_read(_obj, buffer, buffer_size, preserve_internal_buffer);
      }
  };
  
  /**\ingroup cpp_binding
     \brief A wrapper for writeable pMath binary file objects (byte data streams).
   */
  class WriteableBinaryFile: public BinaryFile {
    public:
      WriteableBinaryFile()
        : BinaryFile()
      {
      }
      
      explicit WriteableBinaryFile(pmath_t file_object) throw()
        : BinaryFile(pmath_file_test(file_object, PMATH_FILE_PROP_WRITE) ? pmath_ref(file_object) : PMATH_NULL)
      {
        pmath_unref(file_object);
      }
      
      explicit WriteableBinaryFile(const Expr &file_object) throw()
        : BinaryFile(File(file_object).is_writeable() ? file_object : Expr())
      {
      }
      
      WriteableBinaryFile(const WriteableBinaryFile &src) throw()
        : BinaryFile(src)
      {
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      explicit WriteableBinaryFile(Expr &&file_object) throw()
        : BinaryFile(File(file_object).is_writeable() ? std::move(file_object) : Expr())
      {
      }
      
      WriteableBinaryFile(WriteableBinaryFile &&src) throw()
        : BinaryFile(std::move(src))
      {
      }
#endif
      
      WriteableBinaryFile &operator=(const WriteableBinaryFile &src) {
        BinaryFile::operator=(src);
        return *this;
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      WriteableBinaryFile &operator=(WriteableBinaryFile &&src) {
        BinaryFile::operator=(std::move(src));
        return *this;
      }
#endif
      
      /**\brief Create binary file object whose content is compressed into another binary file. */
      static WriteableBinaryFile create_compressor(WriteableBinaryFile dstfile, struct pmath_compressor_settings_t *settings = nullptr) throw() {
        return WriteableBinaryFile(pmath_file_create_compressor(dstfile.get(), settings));
      }
      
      /**\brief Write some bytes to the file. See pmath_file_write(). */
      size_t write(const void *buffer, size_t buffer_size) throw() {
        return pmath_file_write(_obj, buffer, buffer_size);
      }
      
  };
  
  
  /**\ingroup cpp_binding
     \brief A wrapper for pMath text file objects (byte data streams).
   */
  class TextFile: public File {
    public:
      TextFile()
        : File()
      {
      }
      
      explicit TextFile(pmath_t file_object) throw()
        : File(pmath_file_test(file_object, PMATH_FILE_PROP_TEXT) ? pmath_ref(file_object) : PMATH_NULL)
      {
        pmath_unref(file_object);
      }
      
      explicit TextFile(const Expr &file_object) throw()
        : File(File(file_object).is_text() ? file_object : Expr())
      {
      }
      
      TextFile(const TextFile &src) throw()
        : File(src)
      {
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      explicit TextFile(Expr &&file_object) throw()
        : File(File(file_object).is_text() ? std::move(file_object) : Expr())
      {
      }
      
      TextFile(TextFile &&src) throw()
        : File(std::move(src))
      {
      }
#endif
      
      TextFile &operator=(const TextFile &src) {
        File::operator=(src);
        return *this;
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      TextFile &operator=(TextFile &&src) {
        File::operator=(std::move(src));
        return *this;
      }
#endif
      
      /**\brief Create a text file from a binary file using a known character encoding. */
      static TextFile create_from_binary(BinaryFile binfile, const char *encoding) throw() {
        return TextFile(pmath_file_create_text_from_binary(binfile.release(), encoding));
      }
      
      /**\brief Create a text file from a binary file using UTF-16BE or UTF-16LE, depending on the machine architecture. */
      static TextFile create_from_binary(BinaryFile binfile) {
        return create_from_binary(binfile, PMATH_BYTE_ORDER < 0 ? "UTF-16LE" : "UTF-16BE");
      }
      
      /**\brief Set a file's internal text buffer.
         \param buffer The new line buffer. It should not contain any newline
                       character!
          \see pmath_file_set_textbuffer().
       */
      void set_buffer(String buffer) {
        pmath_file_set_textbuffer(_obj, (pmath_string_t)buffer.release());
      }
  };
  
  /**\ingroup cpp_binding
     \brief A wrapper for pMath readable text file objects (byte data streams).
   */
  class ReadableTextFile: public TextFile {
    public:
      ReadableTextFile()
        : TextFile()
      {
      }
      
      explicit ReadableTextFile(pmath_t file_object) throw()
        : TextFile(pmath_file_test(file_object, PMATH_FILE_PROP_READ) ? pmath_ref(file_object) : PMATH_NULL)
      {
        pmath_unref(file_object);
      }
      
      explicit ReadableTextFile(const Expr &file_object) throw()
        : TextFile(File(file_object).is_readable() ? file_object : Expr())
      {
      }
      
      ReadableTextFile(const ReadableTextFile &src) throw()
        : TextFile(src)
      {
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      explicit ReadableTextFile(Expr &&file_object) throw()
        : TextFile(File(file_object).is_readable() ? std::move(file_object) : Expr())
      {
      }
      
      ReadableTextFile(ReadableTextFile &&src) throw()
        : TextFile(std::move(src))
      {
      }
#endif
      
      ReadableTextFile &operator=(const ReadableTextFile &src) {
        TextFile::operator=(src);
        return *this;
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      ReadableTextFile &operator=(ReadableTextFile &&src) {
        TextFile::operator=(std::move(src));
        return *this;
      }
#endif
      
      /**\brief Create a text file from a binary file using a known character encoding. */
      static ReadableTextFile create_from_binary(ReadableBinaryFile binfile, const char *encoding) throw() {
        return ReadableTextFile(TextFile::create_from_binary(binfile, encoding));
      }
      
      /**\brief Create a text file from a binary file using UTF-16BE or UTF-16LE, depending on the machine architecture. */
      static ReadableTextFile create_from_binary(ReadableBinaryFile binfile) {
        return ReadableTextFile(TextFile::create_from_binary(binfile));
      }
      
      /**\brief Read the next line from the file. */
      String readline() throw() {
        return String(pmath_file_readline(_obj));
      }
      
  };
  
  /**\ingroup cpp_binding
     \brief A wrapper for pMath writeable text file objects (byte data streams).
   */
  class WriteableTextFile: public TextFile {
    public:
      WriteableTextFile() throw()
        : TextFile()
      {
      }
      
      explicit WriteableTextFile(pmath_t file_object) throw()
        : TextFile(pmath_file_test(file_object, PMATH_FILE_PROP_WRITE) ? pmath_ref(file_object) : PMATH_NULL)
      {
        pmath_unref(file_object);
      }
      
      explicit WriteableTextFile(const Expr &file_object) throw()
        : TextFile(File(file_object).is_writeable() ? file_object : Expr())
      {
      }
      
      WriteableTextFile(const WriteableTextFile &src) throw()
        : TextFile(src)
      {
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      explicit WriteableTextFile(Expr &&file_object) throw()
        : TextFile(File(file_object).is_writeable() ? std::move(file_object) : Expr())
      {
      }
      
      WriteableTextFile(WriteableTextFile &&src) throw()
        : TextFile(std::move(src))
      {
      }
#endif
      
      WriteableTextFile &operator=(const WriteableTextFile &src) {
        TextFile::operator=(src);
        return *this;
      }
      
#ifdef PMATH_CPP_USE_RVALUE_REF
      WriteableTextFile &operator=(WriteableTextFile &&src) {
        TextFile::operator=(std::move(src));
        return *this;
      }
#endif
      
      /**\brief Create a text file from a binary file using a known character encoding. */
      static WriteableTextFile create_from_binary(WriteableBinaryFile binfile, const char *encoding) throw() {
        return WriteableTextFile(TextFile::create_from_binary(binfile, encoding));
      }
      
      /**\brief Create a text file from a binary file using UTF-16BE or UTF-16LE, depending on the machine architecture. */
      static WriteableTextFile create_from_binary(WriteableBinaryFile binfile) throw() {
        return WriteableTextFile(TextFile::create_from_binary(binfile));
      }
      
      /**\brief Write some text to the file. */
      void write(const String &str) throw() {
        pmath_file_writetext(_obj, str.buffer(), str.length());
      }
  };
  
  
  
  /**\ingroup cpp_binding
     \brief Abstract base class for C++ callbacks used as pMath files.
  
     The object destructor must be thread-safe (e.g. by not using any global
     data), because it is typically called from another thread than where the
     object was created. If synchronization is needed, it can be done in the
     dereference() callback method.
  
     All other callback methods are synchronized: pMath ensures that no
     callback is entered twice at the same time.
   */
  class UserStream {
    public:
      virtual ~UserStream() {}
      
      /**\brief Test whether a pMath file wraps a user stream.
         \param file The pMath file.
         \return true iff \a file wraps UserStream subclass U object.
       */
      template<class U>
      static bool file_wraps(File file) {
        return manipulate<U>(file, noop);
      }
      
    protected:
      /**\brief Called by pMath when the object is no longer needed.
      
         This method destroys the object by default.
       */
      virtual void dereference() { delete this; }
      
    protected:
      /**\brief Call a method on the user stream behind a pMath file.
         \param file A pMath file that was created from a user stream class U.
         \param callback A member method of the user stream U to be called by
                         pMath.
         \param arg An argument to the callback.
         \return Whether the callback was called. That is, whether the file is
                 actually a user stream of class U.
       */
      template<class U, typename A>
      static bool manipulate(File file, void (U::*callback)(const A &), const A &arg) {
        Manipulator<U, A> manipulator;
        
        manipulator.method = callback;
        manipulator.arg = arg;
        manipulator.success = false;
        
        pmath_file_manipulate(
          file.get(),
          destructor_function< Manipulator<U, A> >,
          Manipulator<U, A>::callback,
          &manipulator);
          
        return manipulator.success;
      }
      
      /**\brief Call a method on the user stream behind a pMath file.
         \param file A pMath file that was created from a user stream class U.
         \param callback A member method of the user stream function Uto be
                         called by pMath.
         \return Whether the callback was called. That is, whether the file is
                 actually a user stream of class U.
       */
      template<class U>
      static bool manipulate(BinaryFile file, void (U::*callback)()) {
        VoidManipulator<U> manipulator;
        
        manipulator.method = callback;
        manipulator.success = false;
        
        pmath_file_manipulate(
          file.get(),
          destructor_function< VoidManipulator<U> >,
          VoidManipulator<U>::callback,
          &manipulator);
          
        return manipulator.success;
      }
      
    protected:
      /**\brief Called by pMath.
       */
      template<class Derived>
      static void destructor_function(void *extra) {
        //UserStream *stream = static_cast<UserStream *>(reinterpret_cast<Derived *>(extra));
        UserStream &stream = (UserStream &)(*reinterpret_cast<Derived *>(extra));
        stream.dereference();
      }
      
    private:
      void noop() {
      }
      
    private:
      template<class C, typename A>
      class Manipulator {
        public:
          void (C::*method)(const A &);
          const A &arg;
          bool success;
          
          static void callback(void *extra, void *data) {
            C                 *obj  = (C *)extra;
            Manipulator<C, A> *info = (Manipulator<C, A> *)data;
            
            (obj->*(info->method))(info->arg);
            info->success = true;
          }
      };
      
      template<class C>
      class VoidManipulator {
        public:
          void (C::*method)();
          bool success;
          
          static void callback(void *extra, void *data) {
            C                  *obj  = (C *)extra;
            VoidManipulator<C> *info = (VoidManipulator<C> *)data;
            
            (obj->*(info->method))();
            info->success = true;
          }
      };
      
  };
  
  /**\ingroup cpp_binding
     \brief Abstract base class for C++ callbacks used as pMath binary files.
   */
  class BinaryUserStream: public UserStream {
    protected:
      /**\brief Called by pMath to check for end-of-file and other errors.
       */
      virtual pmath_files_status_t status() = 0;
      
      /**\brief Called by pMath to flush data to disk.
       */
      virtual void flush() {}
      
      /**\brief Called by pMath to read data.
       */
      virtual size_t read(void *buffer, size_t buffer_size) = 0;
      
      /**\brief Called by pMath to write data.
       */
      virtual size_t write(const void *buffer, size_t buffer_size) = 0;
      
    protected:
      /**\brief Convert to a binary file. pMath will take ownership of the C++ object.
      
         Because pMath now owns the C++ object, you must not touch it directly
         after this call
       */
      BinaryFile convert_to_file(bool readable, bool writeable) {
        pmath_binary_file_api_t api = {0};
        api.struct_size = sizeof(api);
        
        api.status_function = status_function<BinaryUserStream>;
        api.flush_function  = flush_function<BinaryUserStream>;
        if(readable)
          api.read_function = read_function<BinaryUserStream>;
        if(writeable)
          api.write_function = write_function<BinaryUserStream>;
          
        return BinaryFile(pmath_file_create_binary(this, destructor_function<BinaryFile>, &api));
      }
      
      ReadableBinaryFile convert_to_file_readonly() {
        return ReadableBinaryFile(convert_to_file(true, false));
      }
      
      WriteableBinaryFile convert_to_file_writeonly() {
        return WriteableBinaryFile(convert_to_file(false, true));
      }
      
    protected:
      template<class Derived>
      static pmath_files_status_t status_function(void *extra) {
        BinaryUserStream *stream = static_cast<BinaryUserStream *>(reinterpret_cast<Derived *>(extra));
        return stream->status();
      }
      
      template<class Derived>
      static void flush_function(void *extra) {
        BinaryUserStream *stream = static_cast<BinaryUserStream *>(reinterpret_cast<Derived *>(extra));
        return stream->flush();
      }
      
      template<class Derived>
      static size_t read_function(void *extra, void *buffer, size_t buffer_size) {
        BinaryUserStream *stream = static_cast<BinaryUserStream *>(reinterpret_cast<Derived *>(extra));
        return stream->read(buffer, buffer_size);
      }
      
      template<class Derived>
      static size_t write_function(void *extra, const void *buffer, size_t buffer_size) {
        BinaryUserStream *stream = static_cast<BinaryUserStream *>(reinterpret_cast<Derived *>(extra));
        return stream->write(buffer, buffer_size);
      }
  };
  
  /**\ingroup cpp_binding
     \brief Abstract base class for C++ callbacks used as pMath seekable binary
            files.
   */
  class SeekableBinaryUserStream: public BinaryUserStream {
    protected:
      virtual int64_t get_pos() = 0;
      
      virtual pmath_bool_t set_pos(int64_t offset, int origin) = 0;
      
    public:
      /**\brief Convert to a binary file. pMath will take ownership of the C++ object.
      
         Because pMath now owns the C++ object, you must not touch it directly
         after this call
       */
      BinaryFile convert_to_file(bool readable, bool writeable) {
        pmath_binary_file_api_t api = {0};
        api.struct_size = sizeof(api);
        
        api.status_function = status_function<SeekableBinaryUserStream>;
        api.flush_function  = flush_function<SeekableBinaryUserStream>;
        if(readable)
          api.read_function = read_function<SeekableBinaryUserStream>;
        if(writeable)
          api.write_function = write_function<SeekableBinaryUserStream>;
          
        api.get_pos_function = get_pos_function<SeekableBinaryUserStream>;
        api.set_pos_function = set_pos_function<SeekableBinaryUserStream>;
        
        return BinaryFile(pmath_file_create_binary(this, destructor_function<SeekableBinaryUserStream>, &api));
      }
      
    protected:
      template<class Derived>
      static int64_t get_pos_function(void *extra) {
        SeekableBinaryUserStream *stream = static_cast<SeekableBinaryUserStream *>(reinterpret_cast<Derived *>(extra));
        return stream->get_pos();
      }
      
      template<class Derived>
      static pmath_bool_t set_pos_function(void *extra, int64_t offset, int origin) {
        SeekableBinaryUserStream *stream = static_cast<SeekableBinaryUserStream *>(reinterpret_cast<Derived *>(extra));
        return stream->set_pos(offset, origin);
      }
  };
  
  /**\ingroup cpp_binding
     \brief Abstract base class for C++ callbacks used as pMath text files.
   */
  class TextUserStream: public UserStream {
    public:
      /**\brief Called by pMath to check for end-of-file and other errors.
       */
      virtual pmath_files_status_t status() = 0;
      
      /**\brief Called by pMath to flush data to disk.
       */
      virtual void flush() {}
      
      /**\brief Called by pMath to read a line of text, excluding any newline characters.
       */
      virtual String readline() = 0;
      
      /**\brief Called by pMath to write text.
       */
      virtual bool write(const uint16_t *str, int len) = 0;
      
    protected:
      TextFile convert_to_file(bool readable, bool writeable) {
        pmath_text_file_api_t api = {0};
        api.struct_size = sizeof(api);
        
        api.status_function = status_function<TextUserStream>;
        api.flush_function  = flush_function<TextUserStream>;
        if(readable)
          api.readln_function = readln_function<TextUserStream>;
        if(writeable)
          api.write_function = write_function<TextUserStream>;
          
        return TextFile(pmath_file_create_text(this, destructor_function<TextFile>, &api));
      }
      
      ReadableTextFile convert_to_file_readonly() {
        return ReadableTextFile(convert_to_file(true, false));
      }
      
      WriteableTextFile convert_to_file_writeonly() {
        return WriteableTextFile(convert_to_file(false, true));
      }
      
    protected:
      template<class Derived>
      static pmath_files_status_t status_function(void *extra) {
        TextUserStream *stream = static_cast<TextUserStream *>(reinterpret_cast<Derived *>(extra));
        return stream->status();
      }
      
      template<class Derived>
      static void flush_function(void *extra) {
        TextUserStream *stream = static_cast<TextUserStream *>(reinterpret_cast<Derived *>(extra));
        return stream->flush();
      }
      
      template<class Derived>
      static pmath_string_t readln_function(void *extra) {
        TextUserStream *stream = static_cast<TextUserStream *>(reinterpret_cast<Derived *>(extra));
        return stream->readline().release();
      }
      
      template<class Derived>
      static pmath_bool_t write_function(void *extra, const uint16_t *str, int len) {
        TextUserStream *stream = static_cast<TextUserStream *>(reinterpret_cast<Derived *>(extra));
        return stream->write(str, len);
      }
  };
  
  
  
  inline String Expr::to_string(pmath_write_options_t options) const throw() {
    pmath_string_t result = PMATH_NULL;
    
    pmath_write(_obj, options, write_to_string, &result);
    
    return String(result);
  }
  
  inline void Expr::write_to_file(WriteableTextFile file, pmath_write_options_t options) const throw() {
    pmath_file_write_object(file.get(), _obj, options);
  }
  
  inline pmath_serialize_error_t Expr::serialize(WriteableBinaryFile file, int flags) const throw() {
    return pmath_serialize(file.get(), _obj, flags);
  }
  
  inline Expr Expr::deserialize(ReadableBinaryFile file, pmath_serialize_error_t *error) throw() {
    return Expr(pmath_deserialize(file.get(), error));
  }
};


#ifdef PMATH_CPP_SPECIALIZE_STD_HASH

#include <functional>
#define PMATH_DEFINE_STD_HASH_VIA_MEMBER(CLASS) \
  namespace std {                               \
    template<> struct hash<CLASS> {             \
      size_t operator()(const CLASS &obj) {     \
        return obj.hash();                      \
      }                                         \
    };                                          \
  }

PMATH_DEFINE_STD_HASH_VIA_MEMBER(pmath::Expr)
PMATH_DEFINE_STD_HASH_VIA_MEMBER(pmath::String)

#endif

/* @} */

#endif /* __PMATH_CPP_H__ */
