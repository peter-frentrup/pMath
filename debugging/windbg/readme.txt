WinDbg scripts
==============

Inside WinDbg, run 

	$$><C:\full\path\to\pmath\debugging\windbg\init.wds

Later, you can inspect a pmath_t variable `obj` with

	debugpmath inspect  obj

This prints the basic form of obj (strings, symbols, int32, double, expr depth 1).
It presents clickable links for expression elements below depth 1.

If you only have a uint64_t representation of a pmath object, instead of some pmath_t 
symbol, you should run

	debugpmath inspectbits  integer   [caption]

Here, caption is an optional text to descript the location. It is normally a symbol name.


Moreover, the debug build of pmath.dll has some utility functions for the debugger.
Similar to the above macro, there is

	.browse debugpmath unsafeprint obj

You can print the pmath managed stack with
	
	debugpmath stack
	
which is basically an abbreviation for 

	ed pmath!pmath_debug_print_to_debugger 1
	.call pmath!pmath_debug_print_stack(); g
	ed pmath!pmath_debug_print_to_debugger 0

You can redirect all output from pmath_debug_print()/... to the debugger instead of stderr by setting

	ed pmath!pmath_debug_print_to_debugger 1

