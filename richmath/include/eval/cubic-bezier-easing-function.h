#ifndef RICHMATH__EVAL__TRANSITION_FUNCTIONS_H__INCLUDED
#define RICHMATH__EVAL__TRANSITION_FUNCTIONS_H__INCLUDED

namespace richmath {
  // based on https://github.com/gre/bezier-easing/blob/master/src/index.js by Gaëtan Renaudeau 2014 - 2015 – MIT License
  class CubicBezierEasingFunction {
    public:
      CubicBezierEasingFunction(float x1, float y1, float x2, float y2);
      
      bool is_linear() const { return _x1 == _y1 && _x2 == _y2; }
      double get_t_for_x(double x) const;
      double operator()(double x) const;
      
      double x1() const { return _x1; }
      double y1() const { return _y1; }
      double x2() const { return _x2; }
      double y2() const { return _y2; }
      
      static const CubicBezierEasingFunction Linear;
      static const CubicBezierEasingFunction Ease;
      static const CubicBezierEasingFunction EaseIn;
      static const CubicBezierEasingFunction EaseInOut;
      static const CubicBezierEasingFunction EaseOut;
      
    private:
      float _x1;
      float _y1;
      float _x2;
      float _y2;
      
      float _spline_table[11];
    
    public:
      static const int SplineTableSize = 11;
  };
}

#endif // RICHMATH__EVAL__TRANSITION_FUNCTIONS_H__INCLUDED
