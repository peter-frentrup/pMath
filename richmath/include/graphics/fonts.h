#ifndef RICHMATH__GRAPHICS__FONTS_H__INCLUDED
#define RICHMATH__GRAPHICS__FONTS_H__INCLUDED

#include <cairo.h>

#include <util/array.h>
#include <util/pmath-extra.h>
#include <util/hashtable.h>


namespace richmath {
  class FontStyle {
    public:
      FontStyle()
        : italic(0),
          bold(0)
      {}
      
      FontStyle(bool i, bool b)
        : italic(i ? 1u : 0),
          bold(b ? 1u : 0)
      {}
      
      FontStyle &operator|=(const FontStyle &other) {
        italic |=     other.italic;
        bold |=       other.bold;
        return *this;
      }
      
      FontStyle &operator&=(const FontStyle &other) {
        italic &=     other.italic;
        bold &=       other.bold;
        return *this;
      }
      
      FontStyle &operator+=(const FontStyle &other) {
        return *this |= other;
      }
      
      FontStyle &operator-=(const FontStyle &other) {
        italic &=     !other.italic;
        bold &=       !other.bold;
        return *this;
      }
      
      const FontStyle operator|(const FontStyle &other) const {
        FontStyle result(*this);
        return result |= other;
      }
      
      const FontStyle operator&(const FontStyle &other) const {
        FontStyle result(*this);
        return result &= other;
      }
      
      const FontStyle operator+(const FontStyle &other) const {
        FontStyle result(*this);
        return result += other;
      }
      
      const FontStyle operator-(const FontStyle &other) const {
        FontStyle result(*this);
        return result -= other;
      }
      
      const FontStyle operator~() const {
        return FontStyle(!italic, !bold);
      }
      
      const FontStyle operator-() const {
        return ~(*this);
      }
      
      bool operator==(const FontStyle &other) const {
        return italic     == other.italic
               && bold       == other.bold;
      }
      
      bool operator!=(const FontStyle &other) const {
        return !(*this == other);
      }
      
      const char *to_string() const;
      
      // 0 <= result < Permutations
      operator int() const {
        return (int)((italic << 1) | bold);
        //return *reinterpret_cast<const int*>(this);
      }
      
      // 0 <= result < Permutations
      operator unsigned int() const {
        return (italic << 1) | bold;
        //return *reinterpret_cast<const unsigned int*>(this);
      }
      
    public:
      static const int Permutations = 4;
      
      unsigned italic:     1;
      unsigned bold:       1;
  };
  
  extern const FontStyle NoStyle;
  extern const FontStyle Italic;
  extern const FontStyle Bold;
  
  class FontFace {
    public:
      explicit FontFace(cairo_font_face_t *face = nullptr);
      FontFace(const FontFace &face);
      FontFace(
        const String    &name,
        const FontStyle &style);
      ~FontFace();
      
      FontFace &operator=(const FontFace &face);
      
      cairo_font_face_t *cairo() { return _face; }
      
    private:
      cairo_font_face_t *_face;
  };
  
  class FontInfoPrivate;
  class GlyphSubstitutions;
  
#define FONT_TABLE_NAME(a,b,c,d) \
  ( ((uint32_t)(d) << 24) \
    | ((uint32_t)(c) << 16) \
    | ((uint32_t)(b) << 8) \
    |  (uint32_t)(a))
  
  class FontInfo: public Base {
    public:
      FontInfo(FontFace font);
      FontInfo(FontInfo &src);
      ~FontInfo();
      FontInfo &operator=(FontInfo &src);
      
      static Expr all_fonts();
      static bool add_private_font(String filename);
      static void remove_all_private_fonts();
      static bool font_exists(String name, bool exact_match);
      static bool font_exists_exact(String name) {   return font_exists(PMATH_CPP_MOVE(name), true); }
      static bool font_exists_similar(String name) { return font_exists(PMATH_CPP_MOVE(name), false); }
      
      uint16_t char_to_glyph(uint32_t ch);
      
//      uint16_t substitute_single_glyph(
//        uint16_t                    original_glyph,
//        const Array<IndexAndValue> &lookups);
      
      size_t get_truetype_table(
        uint32_t  name,
        size_t    offset,
        void     *buffer,
        size_t    length);
        
      void get_postscript_names(
        Hashtable<String, uint16_t> *name2glyph,
        Hashtable<uint16_t, String> *glyph2name);
        
      const GlyphSubstitutions *get_gsub_table();
      
    private:
      FontInfoPrivate *priv;
  };
};

#endif // RICHMATH__GRAPHICS__FONTS_H__INCLUDED
