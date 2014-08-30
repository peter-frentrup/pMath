#define _WIN32_WINNT 0x501
//#define UNISCRIBE_OPENTYPE  0x0100 /* <- need usp10.dll redistributable for XP */

//#define _WIN32_IE 0x0500
//// or #define SHGFP_TYPE_CURRENT  0

#include <graphics/fonts.h>

#ifdef RICHMATH_USE_WIN32_FONT
#  include <gui/win32/win32-themes.h>
#  include <cairo-win32.h>
#elif defined(RICHMATH_USE_FT_FONT)
#  include <pango/pangocairo.h>
#  define PANGO_ENABLE_ENGINE
#  include <pango/pango-ot.h>
#  include <cairo-ft.h>
#  include FT_TRUETYPE_TABLES_H
#  include <cstdio>
//#  ifdef WIN32
//#    include <shlobj.h>
//#  endif
#else
#  error no support for font backend
#endif

#include <graphics/canvas.h>
#include <graphics/ot-font-reshaper.h>

#include <util/array.h>
#include <util/sharedptr.h>


using namespace richmath;

#ifdef RICHMATH_USE_WIN32_FONT

class AutoDC: public Base {
  public:
    AutoDC(HDC dc): handle(dc) {}
    ~AutoDC() { DeleteDC(handle); }
    HDC handle;
};

static AutoDC dc(CreateCompatibleDC(0));


class PrivateWin32Font: public Shareable {
  public:
    static bool load(String file) {
      file += String::FromChar(0);
      
      guard = new PrivateWin32Font(guard);
      guard->filename.length(file.length());
      memcpy(
        guard->filename.items(),
        file.buffer(),
        guard->filename.length() * sizeof(uint16_t));
        
      if(AddFontResourceExW(guard->filename.items(), FR_PRIVATE, 0) > 0) {
        return true;
      }
      
      guard = guard->next;
      return false;
    }
    
  private:
    PrivateWin32Font(SharedPtr<PrivateWin32Font> _next)
      : Shareable(),
        filename(0),
        next(_next)
    {
    }
    
    ~PrivateWin32Font() {
      if(filename.length() > 0) {
        RemoveFontResourceExW(filename.items(), FR_PRIVATE, 0);
      }
    }
    
  private:
    static SharedPtr<PrivateWin32Font> guard;
    
    Array<WCHAR> filename;
    SharedPtr<PrivateWin32Font> next;
};

SharedPtr<PrivateWin32Font> PrivateWin32Font::guard = 0;

#endif

class StaticCanvas: public Base {
  public:
    StaticCanvas() {
      surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 1, 1);
      cr = cairo_create(surface);
      
      canvas = new Canvas(cr);
    }
    
    ~StaticCanvas() {
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

#ifdef RICHMATH_USE_FT_FONT

class PangoSettings {
  public:
    PangoSettings() {
      font_map = pango_cairo_font_map_new_for_font_type(CAIRO_FONT_TYPE_FT);
      
      if(font_map) {
        pango_cairo_font_map_set_default((PangoCairoFontMap *)font_map);
      }
      else
        fprintf(stderr, "Cannot create Pango font map for Freetype backend.\n");
        
      surface = cairo_surface_reference(static_canvas.surface);
      cr      = cairo_reference(static_canvas.cr);
      context = pango_cairo_create_context(cr);
      
//#ifdef WIN32
//      {
//        char system_font_dir[PATH_MAX];
//        
//        if(FAILED(SHGetFolderPathA(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, system_font_dir))) {
//          fprintf(stderr, "SHGetFolderPathA failed. Using \n");
//          strcpy(system_font_dir, "C:\\Windows\\Fonts");
//        }
//        
//        FcConfigAppFontAddDir(NULL, (const FcChar8*)system_font_dir);
//      }
//#endif
    }
    
    ~PangoSettings() {
      g_object_unref(font_map);
      g_object_unref(context);
      cairo_destroy(cr);
      cairo_surface_destroy(surface);
    }
    
  public:
    cairo_surface_t *surface;
    cairo_t       *cr;
    PangoFontMap  *font_map;
    PangoContext  *context;
};

static PangoSettings pango_settings;

#endif

//{ class FontStyle ...

const FontStyle richmath::NoStyle(false, false);
const FontStyle richmath::Italic(true,  false);
const FontStyle richmath::Bold(false, true);

const char *FontStyle::to_string() const {
  if(italic) {
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
//    PangoFontDescription *desc = pango_font_description_new();
//
//    char *utf8_name = pmath_string_to_utf8(name.get_as_string(), NULL);
//    if(utf8_name)
//      pango_font_description_set_family_static(desc, utf8_name);
//
//    pango_font_description_set_style( desc, style.italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
//    pango_font_description_set_weight(desc, style.bold   ? PANGO_WEIGHT_BOLD  : PANGO_WEIGHT_NORMAL);
//
//    PangoCairoFont *pango_font = (PangoCairoFont*)pango_font_map_load_font(
//      pango_settings.font_map,
//      pango_settings.context,
//      desc);
//
//    cairo_scaled_font_t *scaled_font = pango_cairo_font_get_scaled_font(pango_font);
//    _face = cairo_font_face_reference(cairo_scaled_font_get_font_face(scaled_font));
//
//    pango_font_description_free(desc);
//    pmath_mem_free(utf8_name);
  
    int fcslant  = style.italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN;
    int fcweight = style.bold   ? FC_WEIGHT_BOLD  : FC_WEIGHT_MEDIUM;
    char *family = pmath_string_to_utf8(name.get(), NULL);
  
    FcPattern *pattern = FcPatternBuild(
      NULL,
      FC_FAMILY,     FcTypeString,  family,
      FC_SLANT,      FcTypeInteger, fcslant,
      FC_WEIGHT,     FcTypeInteger, fcweight,
      FC_DPI,        FcTypeDouble,  96.0,
      FC_SCALE,      FcTypeDouble,  0.75,
      FC_SIZE,       FcTypeDouble,  1024.0,
      FC_PIXEL_SIZE, FcTypeDouble,  1024.0 * 0.75,
      FC_SCALABLE,   FcTypeBool,    FcTrue,
      NULL);
  
//    FcResult result;
//    pmath_debug_print("fontset for %s:\n", family);
//    FcFontSet *set = FcFontSort(NULL, pattern, FcFalse, NULL, &result);
//    FcFontSetPrint(set);
//    FcFontSetDestroy(set);
//
//    FcPatternPrint(pattern);
//
//    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
//    FcPatternPrint(pattern);
//
//    FcDefaultSubstitute(pattern);
//    FcPatternPrint(pattern);
//
//    FcPattern *resolved = FcFontMatch(NULL, pattern, &result);
//    FcPatternPrint(resolved);
//
//    FcPatternDestroy(resolved);
  
  
    _face = cairo_ft_font_face_create_for_pattern(pattern);
  
    FcPatternDestroy(pattern);
    pmath_mem_free(family);
  }
#else
#error no support for font backend
#endif
}

FontFace::~FontFace() {
  cairo_font_face_destroy(_face);
}

FontFace &FontFace::operator=(const FontFace &face) {
  cairo_font_face_t *tmp = _face;
  _face = cairo_font_face_reference(face._face);
  cairo_font_face_destroy(tmp);
  return *this;
}

//} ... class FontFace

//{ class FontInfo ...

class richmath::FontInfoPrivate: public Shareable {
  public:
    FontInfoPrivate(FontFace font)
      : Shareable(),
        scaled_font(0),
        gsub_table_data(0)
    {
      static_canvas.canvas->set_font_face(font.cairo());
      scaled_font = cairo_scaled_font_reference(
                      cairo_get_scaled_font(
                        static_canvas.canvas->cairo()));
    }
    
    ~FontInfoPrivate() {
      cairo_scaled_font_destroy(scaled_font);
      free(gsub_table_data);
    }
    
    cairo_font_type_t font_type() {
      return cairo_scaled_font_get_type(scaled_font);
    }
    
  public:
    cairo_scaled_font_t *scaled_font;
    void *gsub_table_data;
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

FontInfo::~FontInfo() {
  priv->unref();
}

FontInfo &FontInfo::operator=(FontInfo &src) {
  if(this != &src) {
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
) {
  Gather::emit(String::FromUcs2((uint16_t *)lpelfe->elfLogFont.lfFaceName));
  return 1;
}
#endif

Expr FontInfo::all_fonts() {
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
#endif
  
#ifdef RICHMATH_USE_FT_FONT
  {
    FcFontSet *app_fonts = FcConfigGetFonts(NULL, FcSetApplication);
    FcFontSet *sys_fonts = FcConfigGetFonts(NULL, FcSetSystem);
    
    int num_app_fonts = app_fonts ? app_fonts->nfont : 0;
    int num_sys_fonts = sys_fonts ? sys_fonts->nfont : 0;
    
    Expr expr = MakeList((size_t)(num_app_fonts + num_sys_fonts));
    
    if(app_fonts) {
      for(int i = 0; i < num_app_fonts; ++i) {
        FcPattern *pattern = app_fonts->fonts[i];
        
        char *name;
        if(pattern && FcPatternGetString(pattern, FC_FAMILY, 0, (FcChar8 **)&name) == FcResultMatch)
          expr.set(i + 1, String::FromUtf8(name));
      }
    }
    
    if(sys_fonts) {
      for(int i = 0; i < num_sys_fonts; ++i) {
        FcPattern *pattern = sys_fonts->fonts[i];
        
        char *name;
        if(pattern && FcPatternGetString(pattern, FC_FAMILY, 0, (FcChar8 **)&name) == FcResultMatch)
          expr.set(i + 1 + num_app_fonts, String::FromUtf8(name));
      }
    }
    
    return expr;
  }
#endif
}

void FontInfo::add_private_font(String filename) {
#ifdef RICHMATH_USE_WIN32_FONT
  PrivateWin32Font::load(filename);
#endif
  
#ifdef RICHMATH_USE_FT_FONT
  {
    char *file = pmath_string_to_utf8(filename.get(), NULL);
    
    FcConfigAppFontAddFile(NULL, (const FcChar8 *)file);
    
    pmath_mem_free(file);
  }
#endif
}

#ifdef RICHMATH_USE_WIN32_FONT
static int CALLBACK search_font(
  ENUMLOGFONTEXW *lpelfe,
  NEWTEXTMETRICEXW *lpntme,
  DWORD FontType,
  LPARAM lParam
) {
  //Gather::emit(String::FromUcs2((uint16_t*)lpelfe->elfLogFont.lfFaceName));
  *(bool *)(lParam) = true;
  return 1;
}
#endif

bool FontInfo::font_exists(String name) {
  if(name.length() == 0)
    return false;
    
#ifdef RICHMATH_USE_WIN32_FONT
  {
    name += String::FromChar(0);
    if(name.length() == 0 || name.length() > LF_FACESIZE)
      return false;
      
    LOGFONTW logfont;
    memset(&logfont, 0, sizeof(logfont));
    logfont.lfCharSet = DEFAULT_CHARSET;
    memcpy(logfont.lfFaceName, name.buffer(), 2 * name.length());
    
    bool found = false;
    
    EnumFontFamiliesExW(
      dc.handle,
      &logfont,
      (FONTENUMPROCW)search_font,
      (LPARAM)&found,
      0);
      
    return found;
  }
#endif
  
#ifdef RICHMATH_USE_FT_FONT
  {
//    PangoFontDescription *desc = pango_font_description_new();
//
//    char *utf8_name = pmath_string_to_utf8(name.get_as_string(), NULL);
//    if(!utf8_name){
//      pango_font_description_free(desc);
//      return false;
//    }
//
//    pango_font_description_set_family_static(desc, utf8_name);
//
//    PangoFont *pango_font = pango_font_map_load_font(
//      pango_settings.font_map,
//      pango_settings.context,
//      desc);
//
//    pango_font_description_free(desc);
//    desc = pango_font_describe(pango_font);
//
//    const char *family = pango_font_description_get_family(desc);
//    pango_font_description_free(desc);
//    if(!family || 0 != strcmp(family, utf8_name)){
//      pmath_mem_free(utf8_name);
//      return false;
//    }
//
//    pmath_mem_free(utf8_name);
//    return true;


    char *family = pmath_string_to_utf8(name.get(), NULL);
    
    FcPattern *pattern = FcPatternBuild(NULL,
                                        FC_FAMILY, FcTypeString, family,
                                        NULL);
                                        
    FcResult result = FcResultMatch;
    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcPattern *resolved = FcFontMatch(NULL, pattern, &result);
    FcPatternDestroy(pattern);
    
    if(result != FcResultMatch) {
      FcPatternDestroy(resolved);
      pmath_mem_free(family);
      return false;
    }
    
    char *str = 0;
    
    if( FcResultTypeMismatch == FcPatternGetString(resolved, FC_FAMILY, 0, (FcChar8 **)&str)
        || !str
        || 0 != strcmp(str, family))
    {
      FcPatternDestroy(resolved);
      pmath_mem_free(family);
      return false;
    }
    
    FcPatternDestroy(resolved);
    pmath_mem_free(family);
    return true;
  }
#endif
  
  return false;
}

uint16_t FontInfo::char_to_glyph(uint32_t ch) {
  switch(priv->font_type()) {
#ifdef RICHMATH_USE_WIN32_FONT
    case CAIRO_FONT_TYPE_WIN32: {
        uint16_t index = 0;
        SaveDC(dc.handle);
        
        cairo_win32_scaled_font_select_font(priv->scaled_font, dc.handle);
        
        if(ch <= 0xFFFF) {
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
        else {
          WCHAR str[2];
          SCRIPT_ITEM uniscribe_items[3];
          int num_items;
          SCRIPT_CACHE cache = NULL;
          WORD           out_glyphs[2];
          SCRIPT_VISATTR vis_attr[  2];
          WORD log_clust[2] = {0, 1};
          int num_glyphs;
          
          ch -= 0x10000;
          str[0] = 0xD800 | (ch >> 10);
          str[1] = 0xDC00 | (ch & 0x3FF);
          
          if(!ScriptItemize(
                str,
                2,
                2,
                NULL,
                NULL,
                uniscribe_items,
                &num_items) &&
              num_items == 1)
          {
            if( !ScriptShape(
                  dc.handle,
                  &cache,
                  str,
                  2,
                  2,
                  &uniscribe_items[0].a,
                  out_glyphs,
                  log_clust,
                  vis_attr,
                  &num_glyphs) &&
                num_glyphs == 1)
            {
              index = out_glyphs[0];
            }
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
        if(face) {
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
  void      *buffer,
  size_t    length
) {
  switch(priv->font_type()) {
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
        if(face) {
          name = ((name & 0xFF000000) >> 24) | ((name & 0xFF0000) >> 8) | ((name & 0xFF00) << 8) | ((name & 0xFF) << 24);
          
          FT_ULong len = length;
          FT_Error err = FT_Load_Sfnt_Table(face, name, offset, (FT_Byte *)buffer, &len);
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
) {
  /* http://developer.apple.com/textfonts/TTRefMan/RM06/Chap6post.html */
  uint8_t arr[4];
  uint32_t pos = 0;
  
  uint32_t size = get_truetype_table(FONT_TABLE_NAME('p', 'o', 's', 't'), 0, 0, 0);
  
  memset(arr, 0, 4);
  get_truetype_table(FONT_TABLE_NAME('p', 'o', 's', 't'), pos, arr, 4);
  pos += 32;
  
  int32_t format = (arr[0] << 24) | (arr[1] << 16) | (arr[2] << 8) | arr[3];
  
  switch(format) {
    case 0x10000: {
        if(name2glyph) {
          for(uint16_t i = 0; i < 257; ++i) {
            name2glyph->set(String(default_postscript_names[i]), i);
          }
        }
        if(glyph2name) {
          for(uint16_t i = 0; i < 257; ++i) {
            glyph2name->set(i, String(default_postscript_names[i]));
          }
        }
      } return;
      
    case 0x20000: {
        memset(arr, 0, 2);
        get_truetype_table(FONT_TABLE_NAME('p', 'o', 's', 't'), pos, arr, 2);
        pos += 2;
        
        uint16_t num_glyphs = (arr[0] << 8) | arr[1];
        Array<uint8_t> glyph_table;
        Array<uint8_t> name_table;
        glyph_table.length(2 * num_glyphs, 0);
        
        if(size > pos) {
          get_truetype_table(
            FONT_TABLE_NAME('p', 'o', 's', 't'),
            pos,
            glyph_table.items(),
            glyph_table.length());
            
          int num_new_glyphs = 0;
          for(int g = 0; g < num_glyphs; ++g) {
            uint16_t index = (glyph_table[2 * g] << 8) | glyph_table[2 * g + 1];
            
            if(index >= 258)
              ++num_new_glyphs;
          }
          
          pos += 2 * num_glyphs;
          name_table.length(size - pos, 0);
          get_truetype_table(
            FONT_TABLE_NAME('p', 'o', 's', 't'),
            pos,
            name_table.items(),
            name_table.length());
            
          Array<String> names(num_new_glyphs);
          int i = 0;
          int n = 0;
          while(n < num_new_glyphs && i < name_table.length()) {
            int len = name_table[i];
            
            const char *s = (const char *)&name_table[i + 1];
            int dotless = 0;
            while(dotless < len && s[dotless] != '.')
              ++dotless;
              
            names[n] = String(s, dotless);
            if(names[n].length() == 0) {
              pmath_debug_print("cannot read glyph name\n");
            }
            
            ++n;
            i += 1 + len;
          }
          
          for(uint16_t g = 0; g < num_glyphs; ++g) {
            uint16_t index = (glyph_table[2 * g] << 8) | glyph_table[2 * g + 1];
            
            if(index < 258) {
              if(name2glyph)
                name2glyph->set(String(default_postscript_names[index]), g);
              if(glyph2name)
                glyph2name->set(g, String(default_postscript_names[index]));
            }
            else {
              index -= 258;
              if(index < num_new_glyphs) {
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

const GlyphSubstitutions *FontInfo::get_gsub_table() {
  if(priv->gsub_table_data == 0) {
    size_t size = get_truetype_table(FONT_TABLE_NAME('G', 'S', 'U', 'B'), 0, 0, 0);
    
    if(size) {
      priv->gsub_table_data = malloc(size);
      
      if(priv->gsub_table_data)
        get_truetype_table(FONT_TABLE_NAME('G', 'S', 'U', 'B'), 0, priv->gsub_table_data, size);
    }
  }
  
  return (const GlyphSubstitutions *)priv->gsub_table_data;
}

//} ... class FontInfo
