#include <util/config.h>
#ifdef RICHMATH_USE_WIN32_FONT

#define _WIN32_WINNT 0x501

#include <graphics/win32-shaper.h>

#include <windows.h>
#include <usp10.h>

#include <cairo-win32.h>

#include <graphics/context.h>
#include <util/array.h>

using namespace richmath;

class AutoDC: public Base{
  public:
    AutoDC(HDC dc): handle(dc){}
    ~AutoDC(){ DeleteDC(handle); }
    HDC handle;
};

static AutoDC dc(CreateCompatibleDC(0));

//{ class WindowsFontShaper ...

WindowsFontShaper::WindowsFontShaper(
  const String  &name,
  FontStyle      style)
: TextShaper(),
  _name(name),
  _style(style),
  _font(name, style)
{
}

WindowsFontShaper::~WindowsFontShaper(){
}

void WindowsFontShaper::decode_token(
  Context        *context,
  int             len,
  const uint16_t *str, 
  GlyphInfo      *result
){
  static Array<uint16_t> indices;
  cairo_text_extents_t cte;
  cairo_glyph_t cg;
  
  indices.length(len);
  
  SaveDC(dc.handle);
  
  context->canvas->set_font_face(_font);
  
  cairo_win32_scaled_font_select_font(
    cairo_get_scaled_font(context->canvas->cairo()),
    dc.handle);
  
  GetGlyphIndicesW(
    dc.handle, 
    (const WCHAR*)str, 
    len, 
    indices.items(),
    GGI_MARK_NONEXISTING_GLYPHS); /* marks nonexistent glyphs with 0xffff = UnknownGlyph */
  
  cg.x = 0;
  cg.y = 0;
  for(int i = 0;i < len;++i){
    if(i + 1 < len
    && is_utf16_high(str[i])
    && is_utf16_low(str[i + 1])){
      SCRIPT_ITEM uniscribe_items[3];
      int num_items;
      
      if(!ScriptItemize(
          (const WCHAR*)str + i,
          2, 
          2,
          NULL,
          NULL,
          uniscribe_items,
          &num_items)
      && num_items == 1)
      {
        SCRIPT_CACHE   cache = NULL;
        WORD           out_glyphs[2];
        SCRIPT_VISATTR vis_attr[  2];
        WORD log_clust[2] = {0,1};
        int num_glyphs;
        
        // todo: use ScriptShapeOpenType when available
        if(!ScriptShape(
            dc.handle,
            &cache,
            (const WCHAR*)str + i + uniscribe_items[0].iCharPos,
            uniscribe_items[1].iCharPos - uniscribe_items[0].iCharPos,
            2,
            &uniscribe_items[0].a,
            out_glyphs,
            log_clust,
            vis_attr,
            &num_glyphs)
        && num_glyphs == 1
        && out_glyphs[0] != 0)
        {
          result[i].index = cg.index = out_glyphs[0];
          
          context->canvas->glyph_extents(&cg, 1, &cte);
          result[i].right = cte.x_advance;
          
          result[i + 1].index = 0;
        }
        else{
          result[i].index = result[i+1].index = UnknownGlyph;
          result[i].right = 0.0;
        }
        
        result[i].fontinfo = 0;
        result[i].composed = 0;
        result[i].x_offset = 0.0;
        
        ++i;
        
        result[i].fontinfo = 0;
        result[i].composed = 0;
        result[i].right    = 0.0;
        result[i].x_offset = 0.0;
        
        continue;
      }
    }
    
    result[i].fontinfo = 0;
    result[i].composed = 0;
    
    result[i].index = cg.index = indices[i];
    
    result[i].x_offset = 0;
    if(!cg.index){
      result[i].right = 0;
    }
    else{
      context->canvas->glyph_extents(&cg, 1, &cte);
      result[i].right = cte.x_advance;
    }
  }
  
  RestoreDC(dc.handle, 1);
}

SharedPtr<TextShaper> WindowsFontShaper::set_style(FontStyle style){
  return find(_name, style);
}

//} ... class WindowsFontShaper

#endif // RICHMATH_USE_WIN32_FONT
