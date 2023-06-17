#ifndef RICHMATH__GUI__CONTROL_PAINTER_H__INCLUDED
#define RICHMATH__GUI__CONTROL_PAINTER_H__INCLUDED

#include <graphics/animation.h>
#include <graphics/margins.h>
#include <graphics/shapers.h>

#include <util/hashtable.h>


namespace richmath {
  class Box;
  class Canvas;
  class StyleData;
  
  enum class ContainerType : uint8_t {
    None,
    FramelessButton,
    GenericButton,
    PushButton,
    DefaultPushButton,
    PaletteButton,
    InputField,
    AddressBandInputField,
    AddressBandBackground,
    AddressBandGoButton,
    TooltipWindow,
    ListViewItem,
    ListViewItemSelected,
    Panel,
    PopupPanel,
    TabHeadAbuttingRight,
    TabHeadAbuttingLeftRight,
    TabHeadAbuttingLeft,
    TabHead,
    TabHeadBackground, // a row of TabPanelTopLeft, TabPanelTopCenter, TabPanelTopRight
    TabBodyBackground, // a 2x3 grid of TabPanelCenterLeft, TabPanelCenter, TabPanelCenterRight and TabPanelBottomLeft, TabPanelBottomCenter, TabPanelBottomRight
    
    TabPanelTopLeft,
    TabPanelTopCenter,
    TabPanelTopRight,
    TabPanelCenterLeft,
    TabPanelCenter,
    TabPanelCenterRight,
    TabPanelBottomLeft,
    TabPanelBottomCenter,
    TabPanelBottomRight,
    
    TabHeadLeftAbuttingBottom,
    TabHeadLeftAbuttingTopBottom,
    TabHeadLeftAbuttingTop,
    TabHeadLeft,
    
    TabHeadRightAbuttingBottom,
    TabHeadRightAbuttingTopBottom,
    TabHeadRightAbuttingTop,
    TabHeadRight,
    
    TabHeadBottomAbuttingRight,
    TabHeadBottomAbuttingLeftRight,
    TabHeadBottomAbuttingLeft,
    TabHeadBottom,
    
    // not really a container:
    HorizontalSliderChannel,
    HorizontalSliderThumb,
    HorizontalSliderUpArrowButton,
    HorizontalSliderDownArrowButton,
    VerticalSliderChannel,
    VerticalSliderThumb,
    VerticalSliderLeftArrowButton,
    VerticalSliderRightArrowButton,
    ToggleSwitchChannelChecked,
    ToggleSwitchThumbChecked,
    ToggleSwitchChannelUnchecked,
    ToggleSwitchThumbUnchecked,
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
  };
  
  enum class ScrollbarPart : uint8_t {
    Nowhere,
    UpLeft,
    LowerRange,
    Thumb,
    UpperRange,
    DownRight,
    SizeGrip
  };
  
  enum class ScrollbarDirection : uint8_t {
    Horizontal,
    Vertical
  };
  
  enum class ControlState : uint8_t {
    Normal,         // mouse avay, not pressed
    Hovered,        // mouse over, not pressed
    Hot,            // mouse over other part of the widget (possibly pressed there)
    Pressed,        // mouse away, pressed
    PressedHovered, // mouse over, pressed
    Disabled        // inactive widget
  };
  
  class ControlContext {
    public:
      virtual bool is_foreground_window() = 0;
      virtual bool is_focused_widget() = 0;
      virtual bool is_using_dark_mode() = 0;
      virtual int dpi() = 0;
      
      static ControlContext &find(Box *box);
    
    public:
      static ControlContext &dummy;
  };
  
  class ControlPainter: public Base {
    public:
      static ControlPainter generic_painter;
      static ControlPainter *std;
      
    public:
      virtual void calc_container_size(
        ControlContext       &control,
        Canvas               &canvas,
        ContainerType         type,
        BoxSize              *extents);
      
      virtual void calc_container_radii(
        ControlContext       &control,
        ContainerType         type,
        BoxRadius            *radii);
      
      virtual Color control_font_color(ControlContext &control, ContainerType type, ControlState state);
      
      static bool is_static_background(ContainerType type) {
        return type == ContainerType::None || type == ContainerType::Panel;
      }
      
      virtual bool is_very_transparent(ControlContext &control, ContainerType type, ControlState state);
      
      virtual void draw_container(
        ControlContext &control,
        Canvas         &canvas,
        ContainerType   type,
        ControlState    state,
        RectangleF      rect);
        
      virtual bool enable_animations() { return true; }
        
      virtual SharedPtr<BoxAnimation> control_transition(
        FrontEndReference  widget_id,
        Canvas            &canvas,
        ContainerType      type1,
        ContainerType      type2,
        ControlState       state1,
        ControlState       state2,
        RectangleF         rect);
        
      virtual Vector2F container_content_offset(
        ControlContext &control, 
        ContainerType   type,
        ControlState    state);
      
      virtual bool container_hover_repaint(ControlContext &control, ContainerType type);
      
      virtual bool control_glow_margins(
        ControlContext &control,
        ContainerType   type,
        ControlState    state,
        Margins<float> *outer,
        Margins<float> *inner);
      
      virtual ContainerType control_glow_type(ControlContext &control, ContainerType type) { return type; }
      
      virtual void paint_scroll_indicator(
        Canvas &canvas,
        Point   pos,
        bool    horz,
        bool    vert);
        
      virtual void system_font_style(ControlContext &control, StyleData *style);
      
      virtual Color selection_color(ControlContext &control);
      
      virtual float scrollbar_width() { return 16 * 3 / 4.f; };
      
      virtual void paint_scrollbar_part(
        ControlContext     &control,
        Canvas             &canvas,
        ScrollbarPart       part,
        ScrollbarDirection  dir,
        ControlState        state,
        RectangleF          rect);
        
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
        ControlContext     &control,
        Canvas             &canvas,
        float               track_pos,
        float               rel_page_size,
        ScrollbarDirection  dir,
        ScrollbarPart       mouseover_part,
        ControlState        state,
        const RectangleF   &rect);
        
    protected:
      ControlPainter();
      virtual ~ControlPainter() {}
  };
}

#endif // RICHMATH__GUI__CONTROL_PAINTER_H__INCLUDED
