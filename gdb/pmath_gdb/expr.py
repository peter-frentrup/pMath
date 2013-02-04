import sys
import gdb
import re
from StringIO import StringIO

class static:
    "Creates a 'static' method"
    def __init__(self, function):
        self.__call__ = function

# from pmath-core/objects.h:
PMATH_TAGMASK_NONDOUBLE = 0x7FF00000
PMATH_TAGMASK_POINTER   = 0xFFF00000
PMATH_TAG_MAGIC         = (PMATH_TAGMASK_NONDOUBLE | 0x10000)
PMATH_TAG_INT32         = (PMATH_TAGMASK_NONDOUBLE | 0x20000)
PMATH_TAG_STR0          = (PMATH_TAGMASK_NONDOUBLE | 0x30000)
PMATH_TAG_STR1          = (PMATH_TAGMASK_NONDOUBLE | 0x40000)
PMATH_TAG_STR2          = (PMATH_TAGMASK_NONDOUBLE | 0x50000)

PMATH_TYPE_SHIFT_MP_FLOAT                = 0
PMATH_TYPE_SHIFT_MP_INT                  = 1
PMATH_TYPE_SHIFT_QUOTIENT                = 2
PMATH_TYPE_SHIFT_BIGSTRING               = 3
PMATH_TYPE_SHIFT_SYMBOL                  = 4
PMATH_TYPE_SHIFT_EXPRESSION_GENERAL      = 5
PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART = 6
PMATH_TYPE_SHIFT_RESERVED_1              = 7
PMATH_TYPE_SHIFT_CUSTOM                  = 8

PMATH_TYPE_MP_INT                  = 1 << PMATH_TYPE_SHIFT_MP_INT
PMATH_TYPE_QUOTIENT                = 1 << PMATH_TYPE_SHIFT_QUOTIENT
PMATH_TYPE_MP_FLOAT                = 1 << PMATH_TYPE_SHIFT_MP_FLOAT
PMATH_TYPE_BIGSTRING               = 1 << PMATH_TYPE_SHIFT_BIGSTRING
PMATH_TYPE_SYMBOL                  = 1 << PMATH_TYPE_SHIFT_SYMBOL
PMATH_TYPE_EXPRESSION_GENERAL      = 1 << PMATH_TYPE_SHIFT_EXPRESSION_GENERAL
PMATH_TYPE_EXPRESSION_GENERAL_PART = 1 << PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART
PMATH_TYPE_EXPRESSION              = PMATH_TYPE_EXPRESSION_GENERAL | PMATH_TYPE_EXPRESSION_GENERAL_PART
PMATH_TYPE_CUSTOM                  = 1 << PMATH_TYPE_SHIFT_CUSTOM

def types_equal_flat(type1, type2):
    type1 = type1.strip_typedefs()
    type2 = type1.strip_typedefs()
    if type1.code != type2.code:
        return False

    if type1.sizeof != type2.sizeof:
        return False

    if type1.code in [gdb.TYPE_CODE_STRUCT, gdb.TYPE_CODE_UNION]:
        fields1 = type1.fields()
        fields2 = type2.fields()
        if len(fields1) != len(fields2):
            return False

        for i in range(len(fields1)):
            if fields1[i].name != fields2[i].name:
                return False

            if not types_equal_flat(fields1[i].type, fields2[i].type):
                return False

    return True
    

class ExprVal:
    _string_header_size = None
    _system_symbol = re.compile('^System`([^`]+)$')
    
    @static
    def string_header_size():
        if ExprVal._string_header_size == None:
            ExprVal._string_header_size = int(gdb.parse_and_eval('((sizeof(struct _pmath_string_t) + sizeof(size_t) - 1) / sizeof(size_t)) * sizeof(size_t)'))
        return ExprVal._string_header_size
        
    def __init__(self, val):
        self._val = val
        self._is_pmath = val != None and types_equal_flat(val.type, gdb.lookup_type('pmath_t'))
        self._tag        = None
        self._type_shift = None

        if self._is_pmath:
            self._tag = int(self._val['s']['tag'])
            
        if self.is_pointer():
            try:
                self._as_pointer = val['as_pointer_64']
            except:
                self._as_pointer = val['s']['u']['as_pointer_32']

            try:
                deref = self._as_pointer.dereference()
                self._type_shift = int(deref['type_shift'].cast(gdb.lookup_type('int')))
            except gdb.MemoryError:
                self._type_shift = None
        else:
            self._as_pointer = gdb.parse_and_eval('(struct _pmath_t*)0x0')
            
    def is_pmath(self):
        return self._is_pmath
        
    def is_double(self):
        return self._is_pmath and ((self._tag & PMATH_TAGMASK_NONDOUBLE) != PMATH_TAGMASK_NONDOUBLE)

    def is_pointer(self):
        return self._is_pmath and ((self._tag & PMATH_TAGMASK_POINTER) == PMATH_TAGMASK_POINTER)

    def is_magic(self):
        return self._is_pmath and (self._tag == PMATH_TAG_MAGIC)

    def is_int32(self):
        return self._is_pmath and (self._tag == PMATH_TAG_INT32)
    
    def is_ministr(self):
        return self._is_pmath and (self._tag in [PMATH_TAG_STR0, PMATH_TAG_STR1, PMATH_TAG_STR2])

    def get_double(self):
        if not self.is_double():
            return float('NaN')
        return self._val['as_double']
    
    def get_int32(self):
        if not self.is_int32():
            return float('NaN')
        return self._val['s']['u']['as_int32']
    
    def get_pointer(self):
        return self._as_pointer
    
    def dereference(self):
        try:
            result = self._as_pointer.dereference()
            fetch_byted = int(result.cast(gdb.lookup_type('char')))
            return result
        except gdb.MemoryError:
            return None

    def get_refcount(self):
        deref = self.dereference()
        if deref == None:
            return 0
        return long(deref['refcount']['_data'])

    def get_pointer_type_shift(self):
        return self._type_shift
    
    def is_pointer_of(self, typ):
        return self._type_shift != None and ((1 << self._type_shift) & typ) != 0

    def is_mpint(self):
        return self.is_pointer_of(PMATH_TYPE_MP_INT)
    
    def is_mpfloat(self):
        return self.is_pointer_of(PMATH_TYPE_MP_FLOAT)
    
    def is_custom(self):
        return self.is_pointer_of(PMATH_TYPE_CUSTOM)
    
    def is_expr(self):
        return self.is_pointer_of(PMATH_TYPE_EXPRESSION)

    def is_float(self):
        return self.is_double() or self.is_mpfloat()

    def is_integer(self):
        return self.is_int32() or self.is_mpint()

    def is_quotient(self):
        return self.is_pointer_of(PMATH_TYPE_QUOTIENT)

    def is_rational(self):
        return self.is_int32() or self.is_pointer_of(PMATH_TYPE_MP_INT | PMATH_TYPE_QUOTIENT)

    def pmath_is_number(self):
        return self.is_int32() or self.is_double() or self.is_pointer_of(PMATH_TYPE_MP_INT | PMATH_TYPE_QUOTIENT | PMATH_TYPE_MP_FLOAT)
    
    def is_bigstr(self):
        return self.is_pointer_of(PMATH_TYPE_BIGSTRING)
    
    def is_string(self):
        return self.is_ministr() or self.is_bigstr()
    
    def is_symbol(self):
        return self.is_pointer_of(PMATH_TYPE_SYMBOL)

    def get_expr_length(self):
        if self.get_refcount() <= 0:
            return 0L

        if self._type_shift in [PMATH_TYPE_SHIFT_EXPRESSION_GENERAL, PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART]:
            expr_data = self.dereference().cast(gdb.lookup_type('struct _pmath_expr_t'))
            return long(expr_data['length'])
    
    def get_expr_item(self, index):
        if index < 0:
            return ExprVal(None)

        if self.get_refcount() <= 0:
            return ExprVal(None)
        
        if self._type_shift == PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:
            expr_data = self.dereference().cast(gdb.lookup_type('struct _pmath_expr_t'))
            length = long(expr_data['length'])
            if index > length:
                return ExprVal(None)
            return ExprVal(expr_data['items'][index])
        
        if self._type_shift == PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART:
            part_data = self.dereference().cast(gdb.lookup_type('struct _pmath_expr_part_t'))
            if index == 0:
                return ExprVal(part_data['inherited']['items'][0])

            length = long(part_data['inherited']['length'])
            if index > length:
                return ExprVal(None)

            try:
                buffer_data = part_data['buffer'].dereference()
            except:
                return ExprVal(None)
            
            start = long(part_data['start'])
            buffer_length = long(buffer_data['length'])
            index = start + index - 1
            
            if index < 0 or index > buffer_length:
                return ExprVal(None)

            return ExprVal(buffer_data['items'][index])
        
        return ExprVal(None)

    def get_string_data(self, errorval = u''):
        if self._tag == PMATH_TAG_STR0:
            return u''

        if self._tag == PMATH_TAG_STR1:
            return unichr(self._val['s']['u']['as_chars'][0])

        if self._tag == PMATH_TAG_STR2:
            return unichr(self._val['s']['u']['as_chars'][0]) + unichr(self._val['s']['u']['as_chars'][1])

        if self._type_shift == PMATH_TYPE_SHIFT_BIGSTRING:
            string_data = self.dereference().cast(gdb.lookup_type('struct _pmath_string_t'))
            length = int(string_data['length'])
            if length < 0:
                return errorval

            if length == 0:
                return u''
            
            cap_or_start = int(string_data['capacity_or_start'])
            if cap_or_start < 0:
                return errorval
            
            try:
                buffer_data = string_data['buffer'].dereference()
                fetch_byted = int(buffer_data.cast(gdb.lookup_type('char')))
            except gdb.MemoryError:
                buffer_data = None

            if buffer_data == None:
                if length > cap_or_start:
                    return errorval
                chars_ptr = self.get_pointer().cast(gdb.lookup_type('void').pointer()) + ExprVal.string_header_size()
            else:
                if buffer_data['length'] < cap_or_start + length:
                    return errorval
                chars_ptr = string_data['buffer'].cast(gdb.lookup_type('void').pointer()) + ExprVal.string_header_size()
                chars_ptr = chars_ptr + cap_or_start
            chars_ptr = chars_ptr.cast(gdb.lookup_type('uint16_t').pointer())
            return u''.join([unichr(int(chars_ptr[i])) for i in range(length)])

        return errorval

    def get_symbol_name(self):
        if not self.is_symbol():
            return u''
        
        symbol_data = self.dereference().cast(gdb.lookup_type('struct _pmath_symbol_t'))
        return ExprVal(symbol_data['name']).get_string_data()
    
    def to_string(self, max_recursion = 3, max_arg_count = 10):
        f = StringIO()
        f.write(u'')
        self.write_to_file(f, max_recursion, max_arg_count)
        s = f.getvalue()
        f.close()
        return s
    
    def write_to_file(self, f, max_recursion, max_arg_count):
        if not self.is_pmath():
            if self._val == None:
                f.write('??')
                return
            f.write('[? {0} ?]'.format(self._val.address))
            return
            
        if self.is_string():
            f.write('"') # u'\u201c'
            f.write(self.get_string_data().replace('\\', '\\\\').replace('"', '\\"').replace('\x00', '\\x00'))
            f.write('"') # u'\u201d'
            return
        
        if self.is_symbol():
            name = self.get_symbol_name()
            match = ExprVal._system_symbol.match(name)
            if match != None:
                f.write(match.group(1))
                return
            f.write(name)
            return

        if self.is_int32():
            f.write(str(self.get_int32()))
            return

        if self.is_double():
            f.write(str(self.get_double()))
            return
            
        if self.is_expr():
            if max_recursion <= 0:
                f.write('...')
                return
            head = self.get_expr_item(0)
            
            length = self.get_expr_length()
            head.write_to_file(f, max_recursion - 1, max_arg_count)
            f.write('(')
            if length > 0:
                until = length
                if length > max_arg_count:
                    until = max_arg_count
                for i in range(1, until):
                    self.get_expr_item(i).write_to_file(f, max_recursion - 1, max_arg_count)
                    f.write(', ')
                if until < length:
                    f.write('...{0} more...'.format(length - until))
                self.get_expr_item(length).write_to_file(f, max_recursion - 1, max_arg_count)
            f.write(')')
            return

        if self.is_pointer():
            if self.get_pointer() == 0:
                f.write('/\\/')
                return
            
            f.write('[{0} at {1}]'.format(self._type_shift, self.get_pointer()))
            return

        f.write('[tag {0} data {1:x}]'.format(self._tag, self._val['s']['u']['as_int32']))
