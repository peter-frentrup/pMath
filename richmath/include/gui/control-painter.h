#ifndef __GUI__CONTROL_PAINTER_H_
#define __GUI__CONTROL_PAINTER_H_

#include <graphics/animation.h>
#include <graphics/shapers.h>

#include <util/hashtable.h>


namespace richmath {
  class Box;
  class Canvas;
  class Style;
  
  typedef enum {
    NoContainerType,
    FramelessButton,
    GenericButton,
    PushButton,
    DefaultPushButton,
    PaletteButton,
    InputField,
    TooltipWindow,
    ListViewItem,
    ListViewItemSelected,
    PanelControl,
    
    // not realy a container:
    SliderHorzChannel,
    SliderHorzThumb,
    ProgressIndicatorBackground,
    ProgressIndicatorBar,
    CheckboxUnchecked,
    CheckboxChecked,
    CheckboxIndeterminate,
    RadioButtonUnchecked,
    RadioButtonChecked,
    OpenerTriangleClosed,
    OpenerTriangleOpened,
    NavigationBack,
    NavigationForward
  } ContainerType;
  
  typedef enum {
    ScrollbarNowhere,
    ScrollbarUpLeft,
    ScrollbarLowerRange,
    ScrollbarThumb,
    ScrollbarUpperRange,
    ScrollbarDownRight,
    ScrollbarSizeGrip
  } ScrollbarPart;
  
  typedef enum {
    ScrollbarHorizontal,
    ScrollbarVertical
  } ScrollbarDirection;
  
  typedef enum {
    Normal,         // mouse avay, not pressed
    Hovered,        // mouse over, not pressed
    Hot,            // mouse over other part of the widget (possibly pressed there)
    Pressed,        // mouse away, pressed
    PressedHovered, // mouse over, pressed
    Disabled        // inactive widget
  } ControlState;
  
  class ControlContext {
    public:
      virtual bool is_foreground_window() = 0;
      virtual int dpi() = 0;
      
      static ControlContext *find(Box *box);
    
    public:
      static ControlContext *dummy;
  };
  
  class ControlPainter: public Base {
    public:
      static ControlPainter generic_painter;
      static ControlPainter *std;
      
    public:
      virtual void calc_container_size(
        ControlContext       *context,
        Canvas               *canvas,
        ContainerType         type,
        BoxSize              *extents);
      
      virtual void calc_container_radii(
        ControlContext       *context,
        ContainerType         type,
        BoxRadius            *radii);
      
      virtual Color control_font_color(ControlContext *context, ContainerType type, ControlState state);
      
      virtual bool is_very_transparent(ControlContext *context, ContainerType type, ControlState state);
      
      virtual void draw_container(
        ControlContext *context,
        Canvas         *canvas,
        ContainerType   type,
        ControlState    state,
        float           x,
        float           y,
        float           width,
        float           height);
        
      virtual SharedPtr<BoxAnimation> control_transition(
        FrontEndReference  widget_id,
        Canvas            *canvas,
        ContainerType      type1,
        ContainerType      type2,
        ControlState       state1,
        ControlState       state2,
        float              x,
        float              y,
        float              width,
        float              height);
        
      virtual void container_content_move(
        ContainerType  type,
        ControlState   state,
        float         *x,
        float         *y);
        
      virtual bool container_hover_repaint(ControlContext *context, ContainerType type);
      
      virtual void paint_scroll_indicator(
        Canvas *canvas,
        float   x,
        float   y,
        bool    horz,
        bool    vert);
        
      virtual void system_font_style(ControlContext *context, Style *style);
      
      virtual Color selection_color(ControlContext *context);
      
      virtual float scrollbar_width() { return 16 * 3 / 4.f; };
      
      virtual void paint_scrollbar_part(
        ControlContext     *context,
        Canvas             *canvas,
        ScrollbarPart       part,
        ScrollbarDirection  dir,
        ControlState        state,
        float               x,
        float               y,
        float               width,
        float               height);
        
      /* scrollbars scroll text, so "upper range" is below current thumb pos:
      
         .---.
         | ^ | up
         |---|              <-- lower_y
         |   | lower range
         |.-.|              <-- thumb_y
         |   | thumb
         |'-'|              <-- upper_y
         |   | upper range
         |---|              <-- down_y
         | v | down
         '---'
      */
      
      void scrollbar_part_pos(
        float  y,
        float  scrollbar_height,
        float  track_pos,     // 0..1
        float  rel_page_size, // =0: default;  >0..=1: relative;  other: no thumb
        float *lower_y,
        float *thumb_y,
        float *upper_y,
        float *down_y);
        
      ScrollbarPart mouse_to_scrollbar_part(
        float rel_mouse,
        float scrollbar_height,
        float track_pos,      // 0..1
        float rel_page_size); // =0: default;  >0..<1: relative;  other: no thumb
        
      void paint_scrollbar(
        ControlContext     *context,
        Canvas             *canvas,
        float               track_pos,
        float               rel_page_size,
        ScrollbarDirection  dir,
        ScrollbarPart       mouseover_part,
        ControlState        state,
        float               x,
        float               y,
        float               width,
        float               height);
        
    protected:
      ControlPainter();
      virtual ~ControlPainter() {}
  };
}

#endif // __GUI__CONTROL_PAINTER_H_
