// You should pre-define PMATH_NUMERICS_SYMBOL(sym, name_str)
// You may pre-define    PMATH_NUMERICS_SYMBOL_DOWNFUNC_IMPL(sym, name_str, func)

#ifdef PMATH_NUMERICS_SYMBOL_DOWNFUNC_IMPL
#  define PMATH_NUMERICS_SYMBOL_DOWNFUNC(sym, name_str, func)  PMATH_NUMERICS_SYMBOL(sym, name_str)  PMATH_NUMERICS_SYMBOL_DOWNFUNC_IMPL(sym, name_str, func)
#else
#  define PMATH_NUMERICS_SYMBOL_DOWNFUNC(sym, name_str, func)  PMATH_NUMERICS_SYMBOL(sym, name_str)
#endif

PMATH_NUMERICS_SYMBOL(         pmath_System_ArcCos,  "System`ArcCos")
PMATH_NUMERICS_SYMBOL(         pmath_System_ArcCosh, "System`ArcCosh")
PMATH_NUMERICS_SYMBOL(         pmath_System_ArcCot,  "System`ArcCot")
PMATH_NUMERICS_SYMBOL(         pmath_System_ArcCoth, "System`ArcCoth")
PMATH_NUMERICS_SYMBOL(         pmath_System_ArcCsc,  "System`ArcCsc")
PMATH_NUMERICS_SYMBOL(         pmath_System_ArcCsch, "System`ArcCsch")
PMATH_NUMERICS_SYMBOL(         pmath_System_ArcSec,  "System`ArcSec")
PMATH_NUMERICS_SYMBOL(         pmath_System_ArcSech, "System`ArcSech")
PMATH_NUMERICS_SYMBOL(         pmath_System_ArcSin,  "System`ArcSin")
PMATH_NUMERICS_SYMBOL(         pmath_System_ArcSinh, "System`ArcSinh")
PMATH_NUMERICS_SYMBOL(         pmath_System_ArcTan,  "System`ArcTan")
PMATH_NUMERICS_SYMBOL(         pmath_System_ArcTanh, "System`ArcTanh")
PMATH_NUMERICS_SYMBOL_DOWNFUNC(pmath_System_Cos,     "System`Cos",      eval_System_Cos)
PMATH_NUMERICS_SYMBOL_DOWNFUNC(pmath_System_Cosh,    "System`Cosh",     eval_System_Cosh)
PMATH_NUMERICS_SYMBOL(         pmath_System_Cot,     "System`Cot")
PMATH_NUMERICS_SYMBOL(         pmath_System_Coth,    "System`Coth")
PMATH_NUMERICS_SYMBOL(         pmath_System_Csc,     "System`Csc")
PMATH_NUMERICS_SYMBOL(         pmath_System_Csch,    "System`Csch")
PMATH_NUMERICS_SYMBOL(         pmath_System_Sec,     "System`Sec")
PMATH_NUMERICS_SYMBOL(         pmath_System_Sech,    "System`Sech")
PMATH_NUMERICS_SYMBOL_DOWNFUNC(pmath_System_Sin,     "System`Sin",      eval_System_Sin)
PMATH_NUMERICS_SYMBOL(         pmath_System_Sinh,    "System`Sinh")
PMATH_NUMERICS_SYMBOL(         pmath_System_Tan,     "System`Tan")
PMATH_NUMERICS_SYMBOL(         pmath_System_Tanh,    "System`Tanh")

#undef PMATH_NUMERICS_SYMBOL_DOWNFUNC
