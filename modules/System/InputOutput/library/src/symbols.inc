// You should pre-define PMATH_IO_SYMBOL(sym, name_str)
// You may pre-define    PMATH_IO_SYMBOL_DOWNFUNC_IMPL(sym, name_str, func)

#ifdef PMATH_IO_SYMBOL_DOWNFUNC_IMPL
#  define PMATH_IO_SYMBOL_DOWNFUNC(sym, name_str, func)  PMATH_IO_SYMBOL(sym, name_str)  PMATH_IO_SYMBOL_DOWNFUNC_IMPL(sym, name_str, func)
#else
#  define PMATH_IO_SYMBOL_DOWNFUNC(sym, name_str, func)  PMATH_IO_SYMBOL(sym, name_str)
#endif

#define PIO_DECLARE_SYSTEM_SYMBOL(name) \
  PMATH_IO_SYMBOL( \
    pmath_System_ ## name, \
    "System`" #name \
  )
#define PIO_DECLARE_SYSTEM_DOLLAR_SYMBOL(name) \
  PMATH_IO_SYMBOL( \
    pmath_System_Dollar ## name, \
    "System`$" #name \
  )
#define PIO_DECLARE_SYSTEM_FUNC(name) \
  PMATH_IO_SYMBOL_DOWNFUNC( \
    pmath_System_ ## name, \
    "System`" #name, \
    eval_System_ ## name \
  )
  
#define PIO_DECLARE_DEVELOPER_FUNC(name) \
  PMATH_IO_SYMBOL_DOWNFUNC( \
    pmath_Developer_ ## name, \
    "Developer`" #name, \
    eval_Developer_ ## name \
  )
  
#define PIO_DECLARE_SYSTEM_PRIVATE_SYMBOL(name) \
  PMATH_IO_SYMBOL( \
    pmath_System_Private_ ## name, \
    "System`Private`" #name \
  )

PIO_DECLARE_SYSTEM_SYMBOL(   ByteCount       )
PIO_DECLARE_SYSTEM_SYMBOL(   DeleteContents  )
PIO_DECLARE_SYSTEM_SYMBOL(   Directory       )
PIO_DECLARE_SYSTEM_SYMBOL(   False           )
PIO_DECLARE_SYSTEM_SYMBOL(   File            )
PIO_DECLARE_SYSTEM_SYMBOL(   FileType        )
PIO_DECLARE_SYSTEM_SYMBOL(   None            )
PIO_DECLARE_SYSTEM_SYMBOL(   OperatingSystem )
PIO_DECLARE_SYSTEM_SYMBOL(   Rule            )
PIO_DECLARE_SYSTEM_SYMBOL(   Special         )
PIO_DECLARE_SYSTEM_SYMBOL(   True            )

PIO_DECLARE_SYSTEM_DOLLAR_SYMBOL( Failed )

PIO_DECLARE_SYSTEM_FUNC(     CopyDirectory    )
PIO_DECLARE_SYSTEM_FUNC(     CopyFile         )
PIO_DECLARE_SYSTEM_FUNC(     CreateDirectory  )
PIO_DECLARE_SYSTEM_FUNC(     DeleteDirectory  )
PIO_DECLARE_SYSTEM_FUNC(     DeleteFile       )
PIO_DECLARE_SYSTEM_FUNC(     FileByteCount    )
PIO_DECLARE_SYSTEM_FUNC(     FileInformation  )
PIO_DECLARE_SYSTEM_FUNC(     FileNameSplit    )
PIO_DECLARE_SYSTEM_FUNC(     RenameDirectory  )
PIO_DECLARE_SYSTEM_FUNC(     RenameFile       )


#undef PIO_DECLARE_SYSTEM_SIMPLIFY_PRIVATE_SYMBOL
#undef PIO_DECLARE_SYSTEM_FUNC
#undef PIO_DECLARE_SYSTEM_SYMBOL
#undef PMATH_IO_SYMBOL_DOWNFUNC
