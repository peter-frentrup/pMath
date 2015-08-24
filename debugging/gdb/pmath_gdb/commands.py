import sys
import gdb
from pmath_gdb.printers import ExprPrinter
from pmath_gdb.expr import ExprVal
from pmath_gdb.util import ManagedStack


class pMathPrefixCommand(gdb.Command):
    "Prefix command for pmath related things."

    def __init__ (self):
        super(pMathPrefixCommand, self).__init__ ("pmath", gdb.COMMAND_SUPPORT, gdb.COMPLETE_NONE, True)

pMathPrefixCommand()


class pMathBacktraceCommand(gdb.Command):
    """print the managed stack"""

    def __init__ (self):
        super(pMathBacktraceCommand, self).__init__ ("pmath bt", gdb.COMMAND_SUPPORT, gdb.COMPLETE_NONE)
    
    def invoke(self, arg, from_tty):
        if arg == '':
            gdb.write(str(ManagedStack.search()))
        else:
            gdb.write(str(ManagedStack.search(int(arg))))
    
pMathBacktraceCommand()


class pMathPrintCommand(gdb.Command):
    """print a pmath variable"""

    def __init__ (self):
        super(pMathPrintCommand, self).__init__ ("pmath p", gdb.COMMAND_SUPPORT, gdb.COMPLETE_NONE)
    
    def invoke(self, arg, from_tty):
        val = gdb.parse_and_eval(arg)
        try:
            expr = ExprVal(val)
        except:
            expr = ExprVal(val['_obj'])
        gdb.write(expr.to_string(max_recursion=ExprPrinter.max_recursion + 1, max_arg_count=2 * ExprPrinter.max_arg_count))
    
pMathPrintCommand()


class pMathSetPrefixCommand(gdb.Command):
    """set a pmath debug flag"""

    def __init__ (self):
        super(pMathSetPrefixCommand, self).__init__ ("pmath set", gdb.COMMAND_SUPPORT, gdb.COMPLETE_NONE, True)
    
pMathSetPrefixCommand()


class pMathSetDepthCommand(gdb.Command):
    """Set the maximum expression depth to use for pretty printing a pmath_t
        Note that "pmath p VAR" uses 1 + that value
    """

    def __init__ (self):
        super(pMathSetDepthCommand, self).__init__ ("pmath set depth", gdb.COMMAND_SUPPORT, gdb.COMPLETE_NONE)
    
    def invoke(self, arg, from_tty):
        try:
            ExprPrinter.max_recursion = int(arg)
        except:
            pass
    
pMathSetDepthCommand()


class pMathSetMaxlenCommand(gdb.Command):
    """Set the maximum expression depth to use for pretty printing a pmath_t
        Note that "pmath p VAR" uses 2 * that value
    """

    def __init__ (self):
        super(pMathSetMaxlenCommand, self).__init__ ("pmath set maxlen", gdb.COMMAND_SUPPORT, gdb.COMPLETE_NONE)
    
    def invoke(self, arg, from_tty):
        try:
            ExprPrinter.max_recursion = int(arg)
        except:
            pass
    
pMathSetMaxlenCommand()


