import sys
import gdb
import re
from pmath_gdb.expr import ExprVal

class static:
    "Creates a 'static' method"
    def __init__(self, function):
        self.__call__ = function

            
pmath_pretty_printers = []
def register_pretty_printer(pretty_printer):
    "Registers a Pretty Printer"
    pmath_pretty_printers.append(pretty_printer)
    return pretty_printer

@register_pretty_printer
class ExprPrinter:
    _regex = re.compile('^pmath::(Expr|String)$')
    max_recursion = 1
    max_arg_count = 10
    
    @static
    def supports(type):
        if ExprVal.type_is_pmath(type):
            return True
            
        if type.tag == None:
            return False
        return ExprPrinter._regex.search(type.tag)
    
##    @static
##    def is_undefine(val):
##        return not ExprVal(val['_obj']).is_pmath()

    def __init__(self, val):
        try:
            self.expr = ExprVal(val['_obj'])
        except gdb.error:
            self.expr = ExprVal(val)

    def to_string(self):
        return self.expr.to_string(max_recursion=ExprPrinter.max_recursion, max_arg_count=ExprPrinter.max_arg_count)

    def children(self):
        ch = {}
        try:
            debug_info = self.expr.get_debug_info()
            if debug_info.is_pmath() and not debug_info.is_null():
                ch['debug info'] = debug_info._val
        except:
            pass
        return ch.items()
    
@register_pretty_printer
class VoidPrinter:
    """print a richmath::Void"""
    
    @static
    def supports(type):
        return type.tag == 'richmath::Void'

    def __init__(self, val):
        pass
    
    def to_string(self):
        return "Void()"

    
@register_pretty_printer
class HashtablePrinter:
    """print a richmath::Hashtable<...>"""
    regex = re.compile('^richmath::Hashtable<.*>$')

    @static
    def supports(type):
        return type.tag != None and HashtablePrinter.regex.search(type.tag)

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return 'richmath::Hashtable with {0} elements'.format(self.val['used_count'])

    class Iterator:
        def __init__(self, table, used_count, capacity):
            self.table = table
            self.used_index = 0
            self.used_count = used_count
            self.index = 0
            self.capacity = capacity
            self.next_value = None

        def __iter__(self):
            return self
        
        def next(self):
            if self.next_value != None:
                v = self.next_value
                self.next_value = None
                return ('[{0} value]'.format(self.index - 1), v)
                
            if self.used_index >= self.used_count:
                raise StopIteration

            while self.index < self.capacity:
                e = self.table[self.index]
                self.index+= 1
                if e.cast(gdb.lookup_type('size_t')) + 1 > 1:
                    self.used_index+= 1
                    self.next_value = e.dereference()['value']
                    return ('[{0} key]'.format(self.index - 1), e.dereference()['key'])

            "oops, data corrupt"
            raise StopIteration
    
    def display_hint (self):
        return 'map'

    def children(self):
        return self.Iterator(self.val['table'], self.val['used_count'], self.val['capacity'])
    

def register_pmath_printers(obj):
    if obj == None:
        obj = gdb

    obj.pretty_printers.append(lookup_function)

# The function looking up the pretty-printer to use for the given value.
def lookup_function(val):
    type = val.type
    type = type.unqualified() #.strip_typedefs()
    for pp in pmath_pretty_printers:
        if pp.supports(type):
            return pp(val)
    return None
