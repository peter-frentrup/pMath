GDB Python scripts
==================

The Code::Blocks projects in pmath.workspace already contain required GDB startup customizations.
When starting GDB, the following GDB macro lines are run in principle:

	python import sys
	python sys.path.insert(0, '/full/path/to/pmath/debugging/gdb')) )
	python import pmath_gdb
	python pmath_gdb.printers.register_pmath_printers(None)

You can use the GDB command `pmath`, see `help pmath`.
To show the managed stack, use `pmath bt`.
To print a pmath_t variable VAR, use `pmath p VAR`.
The output can be customized with `pmath set depth N` and `pmath set maxlen N`.
