#define _WIN32_WINNT 0x501

#include <graphics/fonts.h>
#include <util/config.h>

#ifdef RICHMATH_USE_WIN32_FONT
  #include <windows.h>
  #include <usp10.h>
  #include <cairo-win32.h>
#elif defined(RICHMATH_USE_FT_FONT)
  #include <cairo-ft.h>
  #include <fontconfig.h>
  #include FT_TRUETYPE_TABLES_H
#else
  #error no support for font backend
#endif

#include <graphics/canvas.h>

#include <util/array.h>


using namespace richmath;

#ifdef RICHMATH_USE_WIN32_FONT
  class AutoDC: public Base{
    public:
      AutoDC(HDC dc): handle(dc){}
      ~AutoDC(){ DeleteDC(handle); }
      HDC handle;
  };

  static AutoDC dc(CreateCompatibleDC(0));
#endif

class StaticCanvas: public Base{
  public:
    StaticCanvas(){
      surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 1, 1);
      cr = cairo_create(surface);
      
      canvas = new Canvas(cr);
    }
  
    ~StaticCanvas(){
      delete canvas;
      
      cairo_destroy(cr);
      cairo_surface_destroy(surface);
    }
  
  public:
    cairo_surface_t *surface;
    cairo_t         *cr;
    Canvas          *canvas;
};

static StaticCanvas static_canvas;

//{ class FontStyle ...

const FontStyle richmath::NoStyle(   false, false);
const FontStyle richmath::Italic(    true,  false);
const FontStyle richmath::Bold(      false, true);

const char *FontStyle::to_string() const {
  if(italic){
    if(bold)
      return "Italic + Bold";
    return "Italic";
  }
  if(bold)
    return "Bold";
  return "0";
}

//} ... class FontStyle

//{ class FontFace ...

FontFace::FontFace(cairo_font_face_t *face)
: _face(face)
{
}

FontFace::FontFace(const FontFace &face)
: _face(cairo_font_face_reference(face._face))
{
}

FontFace::FontFace(
  const String    &name,
  const FontStyle &style)
: _face(0)
{
  #ifdef RICHMATH_USE_WIN32_FONT
  {
    LOGFONTW logfontw;
    memset(&logfontw, 0, sizeof(LOGFONTW));
    logfontw.lfWeight         = style.bold ? FW_BOLD : FW_NORMAL;
    logfontw.lfItalic         = style.italic; 
  //  logfontw.lfUnderline      = FALSE; 
  //  logfontw.lfStrikeOut      = FALSE; 
    logfontw.lfCharSet        = DEFAULT_CHARSET; 
    logfontw.lfOutPrecision   = OUT_DEFAULT_PRECIS; 
    logfontw.lfClipPrecision  = CLIP_DEFAULT_PRECIS; 
    logfontw.lfQuality        = DEFAULT_QUALITY; 
    logfontw.lfPitchAndFamily = FF_DONTCARE | DEFAULT_PITCH; 
    int len = name.length();
    if(len >= LF_FACESIZE)
      len = LF_FACESIZE - 1;
    memcpy(logfontw.lfFaceName, name.buffer(), len * sizeof(WCHAR));
    logfontw.lfFaceName[len] = 0;
    
    _face = cairo_win32_font_face_create_for_logfontw(&logfontw);
  }
  #elif defined(RICHMATH_USE_FT_FONT)
  {
    FcPattern *pattern = FcPatternCreate();
    if(pattern){
      char *family = pmath_string_to_utf8(name.get(), NULL);
      
      FcPatternAddString(pattern, FC_FAMILY, (const FcChar8*)family);
      
      pmath_mem_free(family);
      
      int fcslant  = style.italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN;
      FcPatternAddInteger(pattern, FC_SLANT, fcslant);
      
      int fcweight = style.bold   ? FC_WEIGHT_BOLD  : FC_WEIGHT_MEDIUM;
      FcPatternAddInteger(pattern, FC_WEIGHT, fcweight);
    }
    
    _face = cairo_ft_font_face_create_for_pattern(pattern);
  }
  #else
    #error no support for font backend
  #endif
}
        
FontFace::~FontFace(){
  cairo_font_face_destroy(_face);
}
      
FontFace &FontFace::operator=(const FontFace &face){
  cairo_font_face_t *tmp = _face;
  _face = cairo_font_face_reference(face._face);
  cairo_font_face_destroy(tmp);
  return *this;
}

//} ... class FontFace

//{ class FontInfo ...

class richmath::FontInfoPrivate: public Shareable{
  public:
    FontInfoPrivate(FontFace font)
    : Shareable(),
      scaled_font(0)
    {
      static_canvas.canvas->set_font_face(font.cairo());
      scaled_font = cairo_scaled_font_reference(
        cairo_get_scaled_font(
          static_canvas.canvas->cairo()));
    }
    
    ~FontInfoPrivate(){
      cairo_scaled_font_destroy(scaled_font);
    }
    
    cairo_font_type_t font_type(){
      return cairo_scaled_font_get_type(scaled_font);
    }
    
  public:
    cairo_scaled_font_t *scaled_font;
};

FontInfo::FontInfo(FontFace font)
: Base(),
  priv(new FontInfoPrivate(font))
{
  
}

FontInfo::FontInfo(FontInfo &src)
: Base(),
  priv(src.priv)
{
  src.priv->ref();
}

FontInfo::~FontInfo(){
  priv->unref();
}

FontInfo &FontInfo::operator=(FontInfo &src){
  if(this != &src){
    priv->unref();
    src.priv->ref();
    priv = src.priv;
  }
  
  return *this;
}

  #ifdef RICHMATH_USE_WIN32_FONT
    static int CALLBACK emit_font(
      ENUMLOGFONTEXW *lpelfe,
      NEWTEXTMETRICEXW *lpntme,
      DWORD FontType,
      LPARAM lParam
    ){
      Gather::emit(String::FromUcs2((uint16_t*)lpelfe->elfLogFont.lfFaceName));
      return 1;
    }
  #endif

Expr FontInfo::all_fonts(){
  #ifdef RICHMATH_USE_WIN32_FONT
  {
    LOGFONTW logfont;
    memset(&logfont, 0, sizeof(logfont));
    logfont.lfCharSet = DEFAULT_CHARSET;
    logfont.lfFaceName[0] = '\0';
    
    Gather gather;
    
    EnumFontFamiliesExW(
      dc.handle, 
      &logfont, 
      (FONTENUMPROCW)emit_font, 
      0, 
      0);
      
    return gather.end();
  }
  #elif defined(RICHMATH_USE_FT_FONT)
  {
    FcFontSet *font_set = FcConfigGetFonts(NULL, FcSetSystem);
    
    if(!font_set)
      return List();
    
    Expr expr = MakeList((size_t)font_set->nfont);
    for(int i = 0;i < font_set->nfont;++i){
      FcPattern *pattern = font_set->fonts[i];
      
      char *name;
      if(pattern && FcPatternGetString(pattern, FC_FAMILY, 0, (FcChar8**)&name) == FcResultMatch)
        expr.set(i + 1, String::FromUtf8(name));
    }
    
    return expr;
  }
  #endif
}

uint16_t FontInfo::char_to_glyph(uint32_t ch){
  switch(priv->font_type()){
    #ifdef RICHMATH_USE_WIN32_FONT
    case CAIRO_FONT_TYPE_WIN32: {
      uint16_t index = 0;
      SaveDC(dc.handle);
      
      cairo_win32_scaled_font_select_font(priv->scaled_font, dc.handle);
      
      if(ch <= 0xFFFF){
        wchar_t wc = (uint16_t)ch;
        
        GetGlyphIndicesW(
          dc.handle, 
          &wc, 
          1, 
          &index,
          GGI_MARK_NONEXISTING_GLYPHS);
        
        if(index == 0xFFFF)
          index = 0;
      }
      else{
        WCHAR str[2];
        SCRIPT_ITEM uniscribe_items[3];
        int num_items;
        SCRIPT_CACHE cache = NULL;
        WORD           out_glyphs[2];
        SCRIPT_VISATTR vis_attr[  2];
        WORD log_clust[2] = {0,1};
        int num_glyphs;
        
        ch-= 0x10000;
        str[0] = 0xD800 | (ch >> 10);
        str[1] = 0xDC00 | (ch & 0x3FF);
        
        if(!ScriptItemize(
              str,
              2, 
              2,
              NULL,
              NULL,
              uniscribe_items,
              &num_items)
        && num_items == 1
        && !ScriptShape(
              dc.handle,
              &cache,
              str,
              2,
              2,
              &uniscribe_items[0].a,
              out_glyphs,
              log_clust,
              vis_attr,
              &num_glyphs)
        && num_glyphs == 1){
          index = out_glyphs[0];
        }
      }
      
      RestoreDC(dc.handle, 1);
      return index;
    }
    #endif
    
    #ifdef RICHMATH_USE_FT_FONT
    case CAIRO_FONT_TYPE_FT: {
      uint16_t index = 0;
      
      FT_Face face = cairo_ft_scaled_font_lock_face(priv->scaled_font);
      if(face){
        index = FT_Get_Char_Index(face, ch);
      }
      cairo_ft_scaled_font_unlock_face(priv->scaled_font);
      
      return index;
    }
    #endif
    
    default:
      break;
  }
  
  return 0;
}

size_t FontInfo::get_truetype_table(
  uint32_t  name,
  size_t    offset,
  void     *buffer,
  size_t    length
){
  switch(priv->font_type()){
    #ifdef RICHMATH_USE_WIN32_FONT
    case CAIRO_FONT_TYPE_WIN32: {
      cairo_win32_scaled_font_select_font(priv->scaled_font, dc.handle);
      
      DWORD res = GetFontData(
        dc.handle, 
        name, 
        offset,
        buffer,
        length);
      
      if(res == GDI_ERROR)
        return 0;
      return res;
    }
    #endif
    
    #ifdef RICHMATH_USE_FT_FONT
    case CAIRO_FONT_TYPE_FT: {
      FT_Face face = cairo_ft_scaled_font_lock_face(priv->scaled_font);
      if(face){
        name = ((name & 0xFF000000) >> 24) | ((name & 0xFF0000) >> 8) | ((name & 0xFF00) << 8) | ((name & 0xFF) << 24);
      
        FT_ULong len = length;
        FT_Error err = FT_Load_Sfnt_Table(face, name, offset, (FT_Byte*)buffer, &len);
        length = len;
        
        if(err)
          length = 0;
      }
      cairo_ft_scaled_font_unlock_face(priv->scaled_font);
      
      return length;
    }
    #endif
  
    default:
      break;
  }
  
  return 0;
}

static const char *default_postscript_names[258] = {
  ".notdef",
  ".null",
  "nonmarkingreturn",
  "space",
  "exclam",
  "quotedbl",
  "numbersign",
  "dollar",
  "percent",
  "ampersand",
  "quotesingle",
  "parenleft",
  "parenright",
  "asterisk",
  "plus",
  "comma",
  "hyphen",
  "period",
  "slash",
  "zero",
  "one",
  "two",
  "three",
  "four",
  "five",
  "six",
  "seven",
  "eight",
  "nine",
  "colon",
  "semicolon",
  "less",
  "equal",
  "greater",
  "question",
  "at",
  "A",
  "B",
  "C",
  "D",
  "E",
  "F",
  "G",
  "H",
  "I",
  "J",
  "K",
  "L",
  "M",
  "N",
  "O",
  "P",
  "Q",
  "R",
  "S",
  "T",
  "U",
  "V",
  "W",
  "X",
  "Y",
  "Z",
  "bracketleft",
  "backslash",
  "bracketright",
  "asciicircum",
  "underscore",
  "grave",
  "a",
  "b",
  "c",
  "d",
  "e",
  "f",
  "g",
  "h",
  "i",
  "j",
  "k",
  "l",
  "m",
  "n",
  "o",
  "p",
  "q",
  "r",
  "s",
  "t",
  "u",
  "v",
  "w",
  "x",
  "y",
  "z",
  "braceleft",
  "bar",
  "braceright",
  "asciitilde",
  "Adieresis",
  "Aring",
  "Ccedilla",
  "Eacute",
  "Ntilde",
  "Odieresis",
  "Udieresis",
  "aacute",
  "agrave",
  "acircumflex",
  "adieresis",
  "atilde",
  "aring",
  "ccedilla",
  "eacute",
  "egrave",
  "ecircumflex",
  "edieresis",
  "iacute",
  "igrave",
  "icircumflex",
  "idieresis",
  "ntilde",
  "oacute",
  "ograve",
  "ocircumflex",
  "odieresis",
  "otilde",
  "uacute",
  "ugrave",
  "ucircumflex",
  "udieresis",
  "dagger",
  "degree",
  "cent",
  "sterling",
  "section",
  "bullet",
  "paragraph",
  "germandbls",
  "registered",
  "copyright",
  "trademark",
  "acute",
  "dieresis",
  "notequal",
  "AE",
  "Oslash",
  "infinity",
  "plusminus",
  "lessequal",
  "greaterequal",
  "yen",
  "mu",
  "partialdiff",
  "summation",
  "product",
  "pi",
  "integral",
  "ordfeminine",
  "ordmasculine",
  "Omega",
  "ae",
  "oslash",
  "questiondown",
  "exclamdown",
  "logicalnot",
  "radical",
  "florin",
  "approxequal",
  "Delta",
  "guillemotleft",
  "guillemotright",
  "ellipsis",
  "nonbreakingspace",
  "Agrave",
  "Atilde",
  "Otilde",
  "OE",
  "oe",
  "endash",
  "emdash",
  "quotedblleft",
  "quotedblright",
  "quoteleft",
  "quoteright",
  "divide",
  "lozenge",
  "ydieresis",
  "Ydieresis",
  "fraction",
  "currency",
  "guilsinglleft",
  "guilsinglright",
  "fi",
  "fl",
  "daggerdbl",
  "periodcentered",
  "quotesinglbase",
  "quotedblbase",
  "perthousand",
  "Acircumflex",
  "Ecircumflex",
  "Aacute",
  "Edieresis",
  "Egrave",
  "Iacute",
  "Icircumflex",
  "Idieresis",
  "Igrave",
  "Oacute",
  "Ocircumflex",
  "apple",
  "Ograve",
  "Uacute",
  "Ucircumflex",
  "Ugrave",
  "dotlessi",
  "circumflex",
  "tilde",
  "macron",
  "breve",
  "dotaccent",
  "ring",
  "cedilla",
  "hungarumlaut",
  "ogonek",
  "caron",
  "Lslash",
  "lslash",
  "Scaron",
  "scaron",
  "Zcaron",
  "zcaron",
  "brokenbar",
  "Eth",
  "eth",
  "Yacute",
  "yacute",
  "Thorn",
  "thorn",
  "minus",
  "multiply",
  "onesuperior",
  "twosuperior",
  "threesuperior",
  "onehalf",
  "onequarter",
  "threequarters",
  "franc",
  "Gbreve",
  "gbreve",
  "Idotaccent",
  "Scedilla",
  "scedilla",
  "Cacute",
  "cacute",
  "Ccaron",
  "ccaron",
  "dcroat",
};

void FontInfo::get_postscript_names(
  Hashtable<String, uint16_t>            *name2glyph,
  Hashtable<uint16_t, String, cast_hash> *glyph2name
){
  /* http://developer.apple.com/textfonts/TTRefMan/RM06/Chap6post.html */
  uint8_t arr[4];
  uint32_t pos = 0;
  
  uint32_t size = get_truetype_table(FONT_TABLE_NAME('p', 'o', 's', 't'), 0, 0, 0);
  
  memset(arr, 0, 4);
  get_truetype_table(FONT_TABLE_NAME('p', 'o', 's', 't'), pos, arr, 4);
  pos+= 32;
  
  int32_t format = (arr[0] << 24) | (arr[1] << 16) | (arr[2] << 8) | arr[3];
  
  switch(format){
    case 0x10000: {
      if(name2glyph){
        for(uint16_t i = 0;i < 257;++i){
          name2glyph->set(String(default_postscript_names[i]), i);
        }
      }
      if(glyph2name){
        for(uint16_t i = 0;i < 257;++i){
          glyph2name->set(i, String(default_postscript_names[i]));
        }
      }
    } return;
    
    case 0x20000: {
      memset(arr, 0, 2);
      get_truetype_table(FONT_TABLE_NAME('p', 'o', 's', 't'), pos, arr, 2);
      pos+= 2;
      
      uint16_t num_glyphs = (arr[0] << 8) | arr[1];
      Array<uint8_t> glyph_table;
      Array<uint8_t> name_table;
      glyph_table.length(2 * num_glyphs, 0);
      
      if(size > pos){
        get_truetype_table(
          FONT_TABLE_NAME('p', 'o', 's', 't'), 
          pos,
          glyph_table.items(),
          glyph_table.length());
          
        int num_new_glyphs = 0;
        for(int g = 0;g < num_glyphs;++g){
          uint16_t index = (glyph_table[2*g] << 8) | glyph_table[2*g+1];
          
          if(index >= 258)
            ++num_new_glyphs;
        }
        
        pos+= 2 * num_glyphs;
        name_table.length(size - pos, 0);
        get_truetype_table(
          FONT_TABLE_NAME('p', 'o', 's', 't'), 
          pos,
          name_table.items(),
          name_table.length());
        
        Array<String> names(num_new_glyphs);
        int i = 0;
        int n = 0;
        while(n < num_new_glyphs && i < name_table.length()){
          int len = name_table[i];
          
          const char *s = (const char*)&name_table[i+1];
          int dotless = 0;
          while(dotless < len && s[dotless] != '.')
            ++dotless;
            
          names[n] = String(s, dotless);
          if(names[n].length() == 0){
            pmath_debug_print("cannot read glyph name\n");
          }
          
          ++n;
          i+= 1 + len;
        }
        
        for(uint16_t g = 0;g < num_glyphs;++g){
          uint16_t index = (glyph_table[2*g] << 8) | glyph_table[2*g+1];
          
          if(index < 258){
            if(name2glyph)
              name2glyph->set(String(default_postscript_names[index]), g);
            if(glyph2name)
              glyph2name->set(g, String(default_postscript_names[index]));
          }
          else{
            index-= 258;
            if(index < num_new_glyphs){
              if(name2glyph)
                name2glyph->set(names[index], g);
              if(glyph2name)
                glyph2name->set(g, names[index]);
            }
          }
        }
      }
    } return;
  }
}
 
//} ... class FontInfo
