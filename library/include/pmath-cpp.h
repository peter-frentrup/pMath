#ifndef __PMATH_CPP_H__
#define __PMATH_CPP_H__

#include <limits>

#include <pmath.h>

/**\defgroup cpp_binding C++ Binding
   
   There exists a thin layer to easily use pMath with C++. This is usably 
   prefreable over the C API because it handles reference counting/type checking
   automatically and leads to less "boilerplate code"
   
   To use it, simply <tt>\#include <pmath-cpp.h></tt>. The classes are in the
   \c pmath namespace.
   
   This namespace also containts numerous helper functions to easily construct
   expression trees.
   
   @{
 */

namespace pmath{
  class String;
  
  /**\brief A wrapper for pmath_t and drived types.
     \ingroup cpp_binding
     
     This class wraps a single pmath_t, so its size is sizeof(void*).
   */
  class Expr{
    public:
      /**\brief Construct form a pmath_t, stealing the reference. */
      Expr() throw()
      : _obj(PMATH_NULL)
      {
      }
      
      explicit Expr(pmath_t obj) throw()
      : _obj(obj)
      {
      }
      
      Expr(const Expr &src) throw()
      : _obj(pmath_ref(src._obj))
      {
      }
      
      /**\brief Construct from an int. */
      Expr(int i) throw()
      : _obj(pmath_build_value("i", i))
      {
      }
      
      /**\brief Construct from a double. May yield Infinity or Undefined (NaN) values */
      Expr(double f) throw()
      : _obj(pmath_build_value("f", f))
      {
      }
      
      ~Expr() throw() {
        pmath_unref(_obj);
      }
      
      Expr &operator=(const Expr &src) throw() {
        pmath_t tmp = _obj;
        _obj = pmath_ref(src._obj);
        pmath_unref(tmp);
        return *this;
      }
      
      bool is_custom()   const throw() { return pmath_is_custom(  _obj); }
      bool is_double()   const throw() { return pmath_is_double(  _obj); }
      bool is_expr()     const throw() { return pmath_is_expr(    _obj); }
      bool is_float()    const throw() { return pmath_is_float(   _obj); }
      bool is_integer()  const throw() { return pmath_is_integer( _obj); }
      bool is_int32()    const throw() { return pmath_is_int32(   _obj); }
      bool is_magic()    const throw() { return pmath_is_magic(   _obj); }
      bool is_mpfloat()  const throw() { return pmath_is_mpfloat( _obj); }
      bool is_null()     const throw() { return pmath_is_null(    _obj); }
      bool is_number()   const throw() { return pmath_is_number(  _obj); }
      bool is_pointer()  const throw() { return pmath_is_pointer( _obj); }
      bool is_quotient() const throw() { return pmath_is_quotient(_obj); }
      bool is_rational() const throw() { return pmath_is_rational(_obj); }
      bool is_string()   const throw() { return pmath_is_string(  _obj); }
      bool is_symbol()   const throw() { return pmath_is_symbol(  _obj); }
      
      bool is_pointer_of(pmath_type_t type) const throw() { 
        return pmath_is_pointer_of(_obj, type); 
      }
      
      /**\brief Get a hash value. */
      unsigned int hash() const throw() {
        return pmath_hash(_obj);
      }
      
      /**\brief check for identity. The pMath === operator. */
      bool operator==(const Expr &other) const throw() {
        return pmath_equals(_obj, other._obj);
      }
      
      /**\brief check for non-identity. The pMath =!= operator. */
      bool operator!=(const Expr &other) const throw() {
        return !pmath_equals(_obj, other._obj);
      }
      
      /**\brief Compare with another Expr. See pmath_compare() */
      int compare(const Expr &other) const throw() {
        return pmath_compare(_obj, other._obj);
      }
      
      /**\brief Compare with another Expr */
      bool operator<(const Expr &other) const throw() {
        return compare(other) < 0;
      }
      
      /**\brief Compare with another Expr */
      bool operator<=(const Expr &other) const throw() {
        return compare(other) <= 0;
      }
      
      /**\brief Compare with another Expr */
      bool operator>(const Expr &other) const throw() {
        return compare(other) > 0;
      }
      
      /**\brief Compare with another Expr */
      bool operator>=(const Expr &other) const throw() {
        return compare(other) >= 0;
      }
      
      /**\brief Return the pmath_t and discard it. Caller must pmath_unref() it. */
      pmath_t release() throw() { pmath_t o = _obj; _obj = PMATH_NULL; return o; }
      
      /**\brief Get the pmath_t. Reference is held by the Expr object. */
      const pmath_t get() const throw() { return _obj; }
      
      /**\brief Check for null pointer. */
      bool is_valid() const throw() { return !is_null(); }
      
      /**\brief Length of the expression or 0 on error. */
      size_t expr_length() const throw() {
        if(is_expr())
          return pmath_expr_length(_obj);
        return 0;
      }
      
      /**\brief Get the i-th argument of the expression. 
         \param i Index. May be > expr_length().
         \return The i-th argument if the object is a \ref pmath_expr_t.
         expr[0] is the head, expr[1] the first argument and expr[length()] the 
         last argument.
       */
      Expr operator[](size_t i) const throw() {
        if(is_expr())
          return Expr(pmath_expr_get_item(_obj, i));
        return Expr();
      }
      
      /**\brief Change the i-th argument of an expression
         \param i Index. May be > expr_length().
         \param e The new element.
       */
      void set(size_t i, Expr e) throw() {
        if(is_expr())
          _obj = pmath_expr_set_item(_obj, i, e.release());
      }
      
      /**\brief Change the (i,j)-th argument of a matrix
         \param i The matrix row.
         \param j The matrix column.
         \param e The new element.
       */
      void set(size_t i, size_t j, Expr e) throw() {
        if(is_expr()){
          pmath_t item = pmath_expr_extract_item(_obj, i);
          if(pmath_is_expr(item)){
            item = pmath_expr_set_item(item, j, e.release());
            _obj = pmath_expr_set_item(_obj, i, item);
          }
          else{
            _obj = pmath_expr_set_item(_obj, i, item);
          }
        }
      }
      
      /**\brief Convert to a double.
         \param def Optional default value.
         \return the double value if the object is a numeric object and \a def 
                 otherwise.
       */
      double to_double(double def = 0.0) const throw() {
        if(is_number())
          return pmath_number_get_d(_obj);
        
        pmath_t approx = pmath_approximate(
          pmath_ref(_obj), 
          -::std::numeric_limits<double>::infinity(), 
          -::std::numeric_limits<double>::infinity(),
          NULL);
        
        if(pmath_is_number(approx))
          def = pmath_number_get_d(approx);
        
        pmath_unref(approx);
        return def;
      }
      
      /**\brief Convert to a string.
         \param options Optional output options.
         \return The String representation.
       */
      String to_string(pmath_write_options_t options = 0) const throw();
      
    protected:
      pmath_t _obj;
     
    protected:
      static void write_to_string(
        void           *user, 
        const uint16_t *data, 
        int             len
      ) throw() {
        *(pmath_string_t*)user = pmath_string_insert_ucs2(
          *(pmath_string_t*)user, 
          pmath_string_length(*(pmath_string_t*)user), 
          data, 
          len);
      }
  };
  
  /**\brief check for identity. The pMath === operator. 
     \memberof pmath::Expr
   */
  inline bool operator==(pmath_t o1, const Expr &o2){
    return pmath_equals(o1, o2.get());
  }
  
  /**\brief check for non-identity. The pMath =!= operator. 
     \memberof pmath::Expr
   */
  inline bool operator!=(pmath_t o1, const Expr &o2){
    return !pmath_equals(o1, o2.get());
  }
  
  /**\brief check for identity. The pMath === operator. 
     \memberof pmath::Expr
   */
  inline bool operator==(const Expr &o1, pmath_t o2){
    return pmath_equals(o1.get(), o2);
  }
  
  /**\brief check for non-identity. The pMath =!= operator. 
     \memberof pmath::Expr
   */
  inline bool operator!=(const Expr &o1, pmath_t o2){
    return !pmath_equals(o1.get(), o2);
  }
  
  /**\brief A wrapper for pmath_string_t.
     \ingroup cpp_binding
     
     This class provides some string utility functions in addition to Expr.
   */
  class String: public Expr{
    public:
      /**\brief Construct form a pmath_string_t, stealing the reference. */
      String() throw()
      : Expr()
      {
      }
      
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
        if(unicode <= 0xFFFF){
          u16[0] = (uint16_t)unicode;
          return String(pmath_string_insert_ucs2(PMATH_NULL, 0, u16, 1));
        }
        
        if(unicode > 0x10FFFF
        || (unicode & 0xFC00) == 0xD800
        || (unicode & 0xFC00) == 0xDC00)
          return String();
        
        unicode-= 0x10000;
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
      
      /**\brief Append a string. */
      String &operator+=(const String &src) throw() {
        _obj = pmath_string_concat(_obj, (pmath_string_t)pmath_ref(src._obj));
        return *this;
      }
      
      /**\brief Append a C string. */
      String &operator+=(const char *latin1) throw() {
        _obj = pmath_string_insert_latin1(_obj, pmath_string_length(_obj), latin1, -1);
        return *this;
      }
      
      /**\brief Append a UTF-16-string. */
      String &operator+=(const uint16_t *ucs2) throw() {
        _obj = pmath_string_insert_ucs2(_obj, pmath_string_length(_obj), ucs2, -1);
        return *this;
      }
      
      /**\brief Append a single unicode character. */
      String &operator+=(uint16_t ch) throw() {
        _obj = pmath_string_insert_ucs2(_obj, pmath_string_length(_obj), &ch, 1);
        return *this;
      }
      
      /**\brief Concatenate two strings. */
      String operator+(const String &other) const throw() {
        return String(pmath_string_concat(
          (pmath_string_t)pmath_ref(_obj), 
          (pmath_string_t)pmath_ref(other._obj)));
      }
      
      /**\brief Concatenate a String and a C string. */
      String operator+(const char *latin1) const throw() {
        return String(pmath_string_insert_latin1(
          (pmath_string_t)pmath_ref(_obj), 
          pmath_string_length(_obj),
          latin1, 
          -1));
      }
      
      /**\brief Concatenate a String and a UTF-16-string */
      String operator+(const uint16_t *ucs2) const throw() {
        return String(pmath_string_insert_ucs2(
          (pmath_string_t)pmath_ref(_obj), 
          pmath_string_length(_obj),
          ucs2, 
          -1));
      }
      
      /**\brief Concatenate a String and a single unicode character. */
      String operator+(uint16_t ch) const throw() {
        return String(pmath_string_insert_ucs2(
          (pmath_string_t)pmath_ref(_obj), 
          pmath_string_length(_obj),
          &ch, 
          1));
      }
      
      /**\brief Get string part. */
      String part(int start, int length = -1) const throw() {
        return String(pmath_string_part(
          (pmath_string_t)pmath_ref(_obj), start, length));
      }
      
      /**\brief Check for equality with a C string. */
      bool equals(const char *latin1) const throw() {
        return pmath_string_equals_latin1((pmath_string_t)_obj, latin1);
      }
      
      /**\brief Check for prefix equality. */
      bool starts_with(const String &s) const throw() {
        int len = s.length();
        if(len > length())
          return false;
        
        const uint16_t *buf   = buffer();
        const uint16_t *other = s.buffer();
        
        while(len-- > 0)
          if(*buf++ != *other++)
            return false;
        
        return true;
      }
      
      /**\brief Check for prefix equality with a C string (Latin-1). */
      bool starts_with(const char *s, int len = -1) const throw() {
        if(len < 0){
          const char *tmp = s;
          len = 0;
          while(*tmp++)
            ++len;
        }
        
        if(len > length())
          return false;
        
        const uint16_t *buf   = buffer();
        
        while(len-- > 0)
          if(*buf++ != *(unsigned char*)s++)
            return false;
        
        return true;
      }
      
      /**\brief Check for prefix equality with a UCS-2/UTF-16 string. */
      bool starts_with(const uint16_t *s, int len = -1) const throw() {
        if(len < 0){
          const uint16_t *tmp = s;
          len = 0;
          while(*tmp++)
            ++len;
        }
        
        if(len > length())
          return false;
        
        const uint16_t *buf = buffer();
        
        while(len-- > 0)
          if(*buf++ != *s++)
            return false;
        
        return true;
      }
      
      /**\brief Insert a substring. Changes the object itself. */
      void insert(int pos, const String &other) throw() {
        _obj = pmath_string_insert(_obj, pos, pmath_ref(other._obj));
      }
      
      /**\brief Insert a substring. Changes the object itself. */
      void insert(int pos, const char *latin1, int len = -1) throw() {
        _obj = pmath_string_insert_latin1(_obj, pos, latin1, len);
      }
      
      /**\brief Insert a substring. Changes the object itself. */
      void insert(int pos, const uint16_t *ucs2, int len = -1) throw() {
        _obj = pmath_string_insert_ucs2(_obj, pos, ucs2, len);
      }
      
      /**\brief Remove a substring. */
      void remove(int start, int length) throw() {
        pmath_string_t prefix = pmath_string_part(
          (pmath_string_t)pmath_ref(_obj), 0, start);
        
        pmath_string_t postfix = pmath_string_part(
          (pmath_string_t)pmath_ref(_obj), start + length, 
          pmath_string_length(_obj) - start - length);
          
        pmath_unref(_obj);
        _obj = pmath_string_concat(prefix, postfix);
      }
      
      const String trim() const throw() {
        int end = length()-1;
        int start = 0;
        const uint16_t *buf = buffer();
        
        while(start <= end && buf[start] <= ' ')
          ++start;
        
        while(start < end && buf[end] <= ' ')
          --end;
        
        return part(start, end+1-start);
      }
      
      /**\brief Get the string length. */
      int length() const throw() {
        return pmath_string_length(_obj);
      }
      
      /**\brief Get the UCS-2/UTF-16 const string buffer. This is not zero-terminated */
      const uint16_t *buffer() const throw() {
        return pmath_string_buffer(const_cast<pmath_string_t*>(&_obj));
      }
      
      /**\brief Get a single character or U+0000 on error */
      uint16_t operator[](int i) const throw() {
        if(i < 0 || i >= length())
          return 0;
        return buffer()[i];
      }
      
      /**\brief Get the underlying pmath_string_t. It remains owned by this object. */
      const pmath_string_t get_as_string() const throw() { return (pmath_string_t)_obj; }
      
    private:
      Expr &operator=(const Expr &src){ /* not supported */
        return *this;
      }

  };
  
  inline String Expr::to_string(pmath_write_options_t options) const throw() {
    pmath_string_t result = PMATH_NULL;
    
    pmath_write(_obj, options, write_to_string, &result);
    
    return String(result);
  }
  
  /**\brief Utility class for emitting and gathering expressions/building lists 
     \ingroup cpp_binding
     
     Gathering begins with the construction of the object and ends with a call
     to end() or the object destruction.
     
     \note Gathering multiple lists at once is not supported!
    */
  class Gather{
    public:
      /**\brief The constructor. Starts gathering. */
      Gather()
      : ended(false)
      {
        pmath_gather_begin(PMATH_NULL);
      }
      
      explicit Gather(Expr pattern)
      : ended(false)
      {
        pmath_gather_begin(pattern.release());
      }
      
      ~Gather(){
        end();
      }
      
      /**\brief end gathering. */
      Expr end(){
        if(ended) 
          return Expr();
        ended = true;
        return Expr(pmath_gather_end());
      }
      
      /**\brief emit a value to be gathered. */
      static void emit(Expr e){
        pmath_emit(e.release(), PMATH_NULL);
      }
    
      /**\brief emit a value to be gathered. */
      static void emit(Expr e, Expr tag){
        pmath_emit(e.release(), tag.release());
      }
    
    protected:
      bool ended;
      
    private:
      Gather(const Gather &src){
      }
      Gather &operator=(const Gather &src){
        return *this;
      }
  };
  
  
  
  
  inline Expr Number(double d){
    if((double)((int)d) == d)
      return Expr((int)d);
    
    return Expr(d);
  }
  inline Expr Complex(Expr re, Expr im){ return Expr(pmath_build_value("Coo", re.release(), im.release())); }
  inline Expr Imaginary(Expr im){ return Complex(0, im); }
  inline Expr Rational(Expr num, Expr den){ return Expr(pmath_build_value("Qoo", num.release(), den.release())); }
  
  inline Expr Ref(pmath_t o){ return Expr(pmath_ref(o)); }
  inline Expr Symbol(pmath_symbol_t h){ return Ref(h); }
  inline Expr SymbolPi(){ return Symbol(PMATH_SYMBOL_PI); }
  
  inline Expr MakeList(size_t len){ return Expr(pmath_expr_new(pmath_ref(PMATH_SYMBOL_LIST), len)); }
  
  inline Expr Call(Expr h){ 
    return Expr(pmath_expr_new(h.release(), 0)); 
  }
  inline Expr Call(Expr h, Expr x1){ 
    return Expr(pmath_expr_new_extended(h.release(), 1, x1.release())); 
  }
  inline Expr Call(Expr h, Expr x1, Expr x2){ 
    return Expr(pmath_expr_new_extended(h.release(), 2, x1.release(), x2.release())); 
  }
  inline Expr Call(Expr h, Expr x1, Expr x2, Expr x3){ 
    return Expr(pmath_expr_new_extended(h.release(), 3, x1.release(), x2.release(), x3.release())); 
  }
  inline Expr Call(Expr h, Expr x1, Expr x2, Expr x3, Expr x4){ 
    return Expr(pmath_expr_new_extended(h.release(), 4, x1.release(), x2.release(), x3.release(), x4.release())); 
  }
  inline Expr Call(Expr h, Expr x1, Expr x2, Expr x3, Expr x4, Expr x5){ 
    return Expr(pmath_expr_new_extended(h.release(), 5, x1.release(), x2.release(), x3.release(), x4.release(), x5.release())); 
  }
  inline Expr Call(Expr h, Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6){ 
    return Expr(pmath_expr_new_extended(h.release(), 6, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release())); 
  }
  inline Expr Call(Expr h, Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7){ 
    return Expr(pmath_expr_new_extended(h.release(), 7, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release(), x7.release())); 
  }
  inline Expr Call(Expr h, Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7, Expr x8){ 
    return Expr(pmath_expr_new_extended(h.release(), 8, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release(), x7.release(), x8.release())); 
  }
  inline Expr Call(Expr h, Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7, Expr x8, Expr x9){ 
    return Expr(pmath_expr_new_extended(h.release(), 9, x1.release(), x2.release(), x3.release(), x4.release(), x5.release(), x6.release(), x7.release(), x8.release(), x9.release())); 
  }

  inline Expr List(){ 
    return Call(Symbol(PMATH_SYMBOL_LIST)); 
  }
  inline Expr List(Expr x1){ 
    return Call(Symbol(PMATH_SYMBOL_LIST), x1); 
  }
  inline Expr List(Expr x1, Expr x2){ 
    return Call(Symbol(PMATH_SYMBOL_LIST), x1, x2); 
  }
  inline Expr List(Expr x1, Expr x2, Expr x3){ 
    return Call(Symbol(PMATH_SYMBOL_LIST), x1, x2, x3); 
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4){ 
    return Call(Symbol(PMATH_SYMBOL_LIST), x1, x2, x3, x4); 
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5){ 
    return Call(Symbol(PMATH_SYMBOL_LIST), x1, x2, x3, x4, x5); 
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6){ 
    return Call(Symbol(PMATH_SYMBOL_LIST), x1, x2, x3, x4, x5, x6); 
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7){ 
    return Call(Symbol(PMATH_SYMBOL_LIST), x1, x2, x3, x4, x5, x6, x7); 
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7, Expr x8){ 
    return Call(Symbol(PMATH_SYMBOL_LIST), x1, x2, x3, x4, x5, x6, x7, x8); 
  }
  inline Expr List(Expr x1, Expr x2, Expr x3, Expr x4, Expr x5, Expr x6, Expr x7, Expr x8, Expr x9){ 
    return Call(Symbol(PMATH_SYMBOL_LIST), x1, x2, x3, x4, x5, x6, x7, x8, x9); 
  }
  
  inline Expr Rule(       Expr l, Expr r){ return Call(Symbol(PMATH_SYMBOL_RULE),        l, r); }
  inline Expr RuleDelayed(Expr l, Expr r){ return Call(Symbol(PMATH_SYMBOL_RULEDELAYED), l, r); }
  
  inline Expr Power(Expr x, Expr y){ return Call(Symbol(PMATH_SYMBOL_POWER), x, y); }
  
  inline Expr Sqrt(Expr x){ return Power(x, Rational(1, 2)); }
  inline Expr Inv(Expr x){  return Power(x, -1); }
  
  inline Expr Exp(Expr x){  return Power(Symbol(PMATH_SYMBOL_E), x); }
  
  inline Expr Log(Expr x){  return Call(Symbol(PMATH_SYMBOL_LOG), x); }
  inline Expr Log(Expr b, Expr x){ return Call(Symbol(PMATH_SYMBOL_LOG), b, x); }
  
  inline Expr Sin(Expr x){ return Call(Symbol(PMATH_SYMBOL_SIN), x); }
  inline Expr Cos(Expr x){ return Call(Symbol(PMATH_SYMBOL_COS), x); }
  inline Expr Tan(Expr x){ return Call(Symbol(PMATH_SYMBOL_TAN), x); }
  inline Expr ArcSin(Expr x){ return Call(Symbol(PMATH_SYMBOL_ARCSIN), x); }
  inline Expr ArcCos(Expr x){ return Call(Symbol(PMATH_SYMBOL_ARCCOS), x); }
  inline Expr ArcTan(Expr x){ return Call(Symbol(PMATH_SYMBOL_ARCTAN), x); }
  
  inline Expr Times(Expr x1, Expr x2){                   return Call(Symbol(PMATH_SYMBOL_TIMES), x1, x2); }
  inline Expr Times(Expr x1, Expr x2, Expr x3){          return Call(Symbol(PMATH_SYMBOL_TIMES), x1, x2, x3); }
  inline Expr Times(Expr x1, Expr x2, Expr x3, Expr x4){ return Call(Symbol(PMATH_SYMBOL_TIMES), x1, x2, x3, x4); }
  
  inline Expr Divide(Expr x, Expr y){ return Times(x, Inv(y)); }

  inline Expr Plus(Expr x1, Expr x2){                   return Call(Symbol(PMATH_SYMBOL_PLUS), x1, x2); }
  inline Expr Plus(Expr x1, Expr x2, Expr x3){          return Call(Symbol(PMATH_SYMBOL_PLUS), x1, x2, x3); }
  inline Expr Plus(Expr x1, Expr x2, Expr x3, Expr x4){ return Call(Symbol(PMATH_SYMBOL_PLUS), x1, x2, x3, x4); }

  inline Expr Minus(Expr x){         return Times(-1, x); }
  inline Expr Minus(Expr x, Expr y){ return Plus(x, Minus(y)); }
  
  inline Expr Abs( Expr x){ return Call(Symbol(PMATH_SYMBOL_ABS),  x); }
  inline Expr Arg( Expr x){ return Call(Symbol(PMATH_SYMBOL_ARG),  x); }
  inline Expr Sign(Expr x){ return Call(Symbol(PMATH_SYMBOL_SIGN), x); }
  inline Expr Re(  Expr x){ return Call(Symbol(PMATH_SYMBOL_RE),   x); }
  inline Expr Im(  Expr x){ return Call(Symbol(PMATH_SYMBOL_IM),   x); }
  
  inline Expr Ceiling(Expr x){         return Call(Symbol(PMATH_SYMBOL_CEILING), x); }
  inline Expr Ceiling(Expr x, Expr a){ return Call(Symbol(PMATH_SYMBOL_CEILING), x, a); }
  inline Expr Floor(  Expr x){         return Call(Symbol(PMATH_SYMBOL_FLOOR),   x); }
  inline Expr Floor(  Expr x, Expr a){ return Call(Symbol(PMATH_SYMBOL_FLOOR),   x, a); }
  inline Expr Round(  Expr x){         return Call(Symbol(PMATH_SYMBOL_ROUND),   x); }
  inline Expr Round(  Expr x, Expr a){ return Call(Symbol(PMATH_SYMBOL_ROUND),   x, a); }
  
  inline Expr Quotient(Expr m, Expr n){         return Call(Symbol(PMATH_SYMBOL_QUOTIENT), m, n); }
  inline Expr Quotient(Expr m, Expr n, Expr d){ return Call(Symbol(PMATH_SYMBOL_QUOTIENT), m, n, d); }
  inline Expr Mod(     Expr m, Expr n){         return Call(Symbol(PMATH_SYMBOL_MOD),      m, n); }
  inline Expr Mod(     Expr m, Expr n, Expr d){ return Call(Symbol(PMATH_SYMBOL_MOD),      m, n, d); }
  
  
  inline Expr Evaluate(Expr x){ return Expr(pmath_evaluate(x.release())); }
  inline Expr ParseArgs(const char *code, Expr arglist){
    return Expr(pmath_parse_string_args(
      code,
      "o",
      arglist.release())); 
  }
  inline Expr Parse(const char *code){ return Expr(pmath_parse_string(PMATH_C_STRING(code))); }
  inline Expr Parse(const char *code, Expr x1){
    return ParseArgs(code, List(x1)); 
  }
  inline Expr Parse(const char *code, Expr x1, Expr x2){
    return ParseArgs(code, List(x1, x2)); 
  }
  inline Expr Parse(const char *code, Expr x1, Expr x2, Expr x3){
    return ParseArgs(code, List(x1, x2, x3)); 
  }
  inline Expr Parse(const char *code, Expr x1, Expr x2, Expr x3, Expr x4){
    return ParseArgs(code, List(x1, x2, x3, x4)); 
  }
};

/* @} */

#endif /* __PMATH_CPP_H__ */
