#ifndef RICHMATH__GRAPHICS__CAIROSTREAM_H__INCLUDED
#define RICHMATH__GRAPHICS__CAIROSTREAM_H__INCLUDED

#include <util/base.h>
#include <pmath-cpp.h>
#include <cairo.h>


namespace richmath {

  class CairoStream: public Base {
    public:
      static cairo_surface_t *create_svg_surface(
        pmath::WriteableBinaryFile file,
        double                     width_in_points,
        double                     height_in_points);
        
      static cairo_surface_t *create_pdf_surface(
        pmath::WriteableBinaryFile file,
        double                     width_in_points,
        double                     height_in_points);
        
      static CairoStream *from_surface(cairo_surface_t *surface);
      
      pmath::String copy_to_string_utf8();
      
    public:
      CairoStream();
      CairoStream(pmath::WriteableBinaryFile _file);
      virtual ~CairoStream();
      
      static void cairo_destroy_func(void *self);
      static cairo_status_t cairo_write_func(void *self, const unsigned char *data, unsigned int length);
      
      // frees this
      void attach_to_surface(cairo_surface_t *surface);
      
          
    public:
      pmath::WriteableBinaryFile file;
  };
};

#endif // RICHMATH__GRAPHICS__CAIROSTREAM_H__INCLUDED
