import sys
import gdb
import re
from pmath_gdb.expr import ExprVal, ExprFormatting

            
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
    
    @staticmethod
    def supports(type):
        if ExprVal.type_is_pmath(type):
            return True
            
        if type.tag == None:
            return False
        return ExprPrinter._regex.search(type.tag)
    
##    @staticmethod
##    def is_undefine(val):
##        return not ExprVal(val['_obj']).is_pmath()

    def __init__(self, val):
        try:
            self.expr = ExprVal(val['_obj'])
        except gdb.error:
            self.expr = ExprVal(val)

    def to_string(self):
        #if self.expr.is_string():
        #    return self.expr.get_string_data().encode('unicode-escape').decode('latin1')
        string = self.expr.to_string(max_recursion=ExprPrinter.max_recursion, max_arg_count=ExprPrinter.max_arg_count)
        #string = string.replace('\\', '/')
        string = string.encode('unicode-escape').decode('latin1').replace('\\\\', '\\')
        return string

    def display_hint (self):
        #if self.expr.is_string():
        #return 'string'
        return 'expression'
    
    def children(self):
        ch = {}
        try:
            debug_metadata = self.expr.get_debug_metadata()
            if debug_metadata.is_pmath() and not debug_metadata.is_null():
                ch['debug info'] = ExprFormatting.debug_source_info_to_string(debug_metadata)
        except:
            pass
        if self.expr.is_pointer():
            ch['pointer'] = self.expr.get_pointer()
            ch['refcount'] = gdb.Value(self.expr.get_refcount()).cast(gdb.lookup_type('intptr_t'))
            
        return ch.items()
    
@register_pretty_printer
class VoidPrinter:
    """print a richmath::Void"""
    
    @staticmethod
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

    @staticmethod
    def supports(type):
        return type.tag != None and HashtablePrinter.regex.search(type.tag)

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return 'richmath::Hashtable with {0} elements'.format(self.val['used_count'])

    def _table(self):
        large_table = self.val['large_table']
        if int(large_table) != 0:
            return large_table
        else:
            return self.val['small_table']

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
        
        def __next__(self):
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
    
    def display_hint(self):
        return 'map'

    def children(self):
        return self.Iterator(self._table(), self.val['used_count'], self.val['capacity'])
    

@register_pretty_printer
class ArrayPrinter:
    """print a richmath::Array<...>"""
    regex = re.compile('^richmath::Array<.*>$')
    
    @staticmethod
    def supports(type):
        return type.tag != None and ArrayPrinter.regex.search(type.tag)

    def __init__(self, val):
        self.val = val

    def display_hint (self):
        return 'array'

    def _length(self):
        if int(self.val["_items"]) == 0:
            return 0
        return int(self.val["_items"].cast(gdb.lookup_type('int').pointer())[-1])
    
    def to_string(self):
        return 'richmath::Array of {0} elements'.format(self._length())
    
    class Iterator:
        def __init__(self, items, length):
            self.items = items
            self.index = 0
            self.length = length

        def __iter__(self):
            return self
        
        def __next__(self):
            if self.index >= self.length:
                raise StopIteration
            
            result = self.items[self.index]
            self.index+= 1
            return (str(self.index), result)
    
    def children(self):
        return self.Iterator(self.val['_items'], self._length())
    


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
