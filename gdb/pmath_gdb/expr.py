import sys
import gdb
import re
from StringIO import StringIO

class static:
    "Creates a 'static' method"
    def __init__(self, function):
        self.__call__ = function

# from pmath-core/objects.h and pmath-core/objects-private.h:
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
PMATH_TYPE_SHIFT_MULTIRULE               = 7
PMATH_TYPE_SHIFT_CUSTOM                  = 8
PMATH_TYPE_SHIFT_BLOB                    = 9
PMATH_TYPE_SHIFT_PACKED_ARRAY            = 10
PMATH_TYPE_SHIFT_INTERVAL                = 11

PMATH_TYPE_MP_INT                  = 1 << PMATH_TYPE_SHIFT_MP_INT
PMATH_TYPE_QUOTIENT                = 1 << PMATH_TYPE_SHIFT_QUOTIENT
PMATH_TYPE_MP_FLOAT                = 1 << PMATH_TYPE_SHIFT_MP_FLOAT
PMATH_TYPE_BIGSTRING               = 1 << PMATH_TYPE_SHIFT_BIGSTRING
PMATH_TYPE_SYMBOL                  = 1 << PMATH_TYPE_SHIFT_SYMBOL
PMATH_TYPE_EXPRESSION_GENERAL      = 1 << PMATH_TYPE_SHIFT_EXPRESSION_GENERAL
PMATH_TYPE_EXPRESSION_GENERAL_PART = 1 << PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART
PMATH_TYPE_PACKED_ARRAY            = 1 << PMATH_TYPE_SHIFT_PACKED_ARRAY
PMATH_TYPE_EXPRESSION              = PMATH_TYPE_EXPRESSION_GENERAL | PMATH_TYPE_EXPRESSION_GENERAL_PART | PMATH_TYPE_PACKED_ARRAY
PMATH_TYPE_MULTIRULE               = 1 << PMATH_TYPE_SHIFT_MULTIRULE
PMATH_TYPE_CUSTOM                  = 1 << PMATH_TYPE_SHIFT_CUSTOM
PMATH_TYPE_BLOB                    = 1 << PMATH_TYPE_SHIFT_BLOB
PMATH_TYPE_INTERVAL                = 1 << PMATH_TYPE_SHIFT_INTERVAL

PMATH_PACKED_DOUBLE = 1
PMATH_PACKED_INT32  = 2


class ExprVal:
    _string_header_size = None
    _system_symbol = re.compile('^System`([^`]+)$')
    _type_shift_names = {
        PMATH_TYPE_SHIFT_MP_FLOAT:                'mp float',
        PMATH_TYPE_SHIFT_MP_INT:                  'mp int',
        PMATH_TYPE_SHIFT_QUOTIENT:                'quotient',
        PMATH_TYPE_SHIFT_BIGSTRING:               'string',
        PMATH_TYPE_SHIFT_SYMBOL:                  'symbol',
        PMATH_TYPE_SHIFT_EXPRESSION_GENERAL:      'expression',
        PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART: 'expression part',
        PMATH_TYPE_SHIFT_MULTIRULE:               'multirule',
        PMATH_TYPE_SHIFT_CUSTOM:                  'custom',
        PMATH_TYPE_SHIFT_BLOB:                    'blob',
        PMATH_TYPE_SHIFT_PACKED_ARRAY:            'packed array',
        PMATH_TYPE_SHIFT_INTERVAL:                'interval'}
    _tag_names = {
        PMATH_TAG_MAGIC: 'magic',
        PMATH_TAG_INT32: 'int32',
        PMATH_TAG_STR0:  'str0',
        PMATH_TAG_STR1:  'str1',
        PMATH_TAG_STR2:  'str2'}
    _known_symbol_indices = {
        'System`List': 1}
    
    @static
    def type_is_pmath(type):
        type = type.strip_typedefs()
        return type.code == gdb.TYPE_CODE_UNION and type.tag == 'pmath_t'
    
    @static
    def string_header_size():
        if ExprVal._string_header_size == None:
            _pmath_string_t = gdb.lookup_type('struct _pmath_string_t')
            size_t          = gdb.lookup_type('size_t')
            ExprVal._string_header_size = ((_pmath_string_t.sizeof + size_t.sizeof - 1) // size_t.sizeof) * size_t.sizeof
        return ExprVal._string_header_size
    
    @static
    def from_pointer(ptrval):
        if ptrval.type.code != gdb.TYPE_CODE_PTR:
            return ExprVal(None)
        
        # currently only 32bit little endian code:
        pmath_t = gdb.lookup_type('pmath_t')
        uint64_t = gdb.lookup_type('uint64_t')
        ptrval = ptrval.reinterpret_cast(gdb.lookup_type('size_t')).cast(uint64_t)
        tagval = gdb.Value(PMATH_TAGMASK_POINTER).cast(uint64_t)
        val = (tagval << 32) | ptrval
        val = val.cast(pmath_t)
        return ExprVal(val)
    
    @static
    def get_builtin_symbol(indexOrKnownName):
        if isinstance(indexOrKnownName, str):
            if ExprVal._known_symbol_indices.has_key(indexOrKnownName):
                indexOrKnownName = ExprVal._known_symbol_indices[indexOrKnownName]
            else:
                return ExprVal(None)
        
        return ExprVal(gdb.lookup_global_symbol('_pmath_builtin_symbol_array').value()[indexOrKnownName])
        
    def __init__(self, val):
        self._val = val
        self._is_pmath = val != None and ExprVal.type_is_pmath(val.type)
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
            self._as_pointer = gdb.Value(0).cast(gdb.lookup_type('struct _pmath_t').pointer())
            
    def is_pmath(self):
        return self._is_pmath
        
    def is_double(self):
        return self._is_pmath and ((self._tag & PMATH_TAGMASK_NONDOUBLE) != PMATH_TAGMASK_NONDOUBLE)

    def is_pointer(self):
        return self._is_pmath and ((self._tag & PMATH_TAGMASK_POINTER) == PMATH_TAGMASK_POINTER)

    def is_null(self):
        return self._is_pmath and self._as_pointer == 0

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
    
    def is_multirule(obj):
        return self.is_pointer_of(PMATH_TYPE_MULTIRULE)
    
    def is_custom(self):
        return self.is_pointer_of(PMATH_TYPE_CUSTOM)
    
    def is_blob(self):
        return self.is_pointer_of(PMATH_TYPE_BLOB)

    def is_packed_array(self):
        return self.is_pointer_of(PMATH_TYPE_PACKED_ARRAY)

    def is_expr(self):
        return self.is_pointer_of(PMATH_TYPE_EXPRESSION)

    def is_interval(self):
        return self.is_pointer_of(PMATH_TYPE_INTERVAL)

    def is_float(self):
        return self.is_double() or self.is_mpfloat()

    def is_integer(self):
        return self.is_int32() or self.is_mpint()

    def is_quotient(self):
        return self.is_pointer_of(PMATH_TYPE_QUOTIENT)

    def is_rational(self):
        return self.is_int32() or self.is_pointer_of(PMATH_TYPE_MP_INT | PMATH_TYPE_QUOTIENT)

    def is_number(self):
        return self.is_int32() or self.is_double() or self.is_pointer_of(PMATH_TYPE_MP_INT | PMATH_TYPE_QUOTIENT | PMATH_TYPE_MP_FLOAT)
    
    def is_bigstr(self):
        return self.is_pointer_of(PMATH_TYPE_BIGSTRING)
    
    def is_string(self):
        return self.is_ministr() or self.is_bigstr()
    
    def is_symbol(self):
        return self.is_pointer_of(PMATH_TYPE_SYMBOL)
    
    def is_expr_of(self, name, length = []):
        if not self.is_expr():
            return False
        
        l = self.get_expr_length()
        if l != length and l not in length:
            return False
        
        head = self.get_expr_item(0)
        if not head.is_symbol():
            return False
        
        return head.get_symbol_name() == name
    
    def get_expr_length(self):
        if self.get_refcount() <= 0:
            return 0L

        if self._type_shift in [PMATH_TYPE_SHIFT_EXPRESSION_GENERAL, PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART]:
            expr_data = self.dereference().cast(gdb.lookup_type('struct _pmath_expr_t'))
            return long(expr_data['length'])
        
        if self._type_shift == PMATH_TYPE_SHIFT_PACKED_ARRAY:
            return self.get_packed_array_sizes()[0]
    
    def get_expr_item(self, index):
        # packed arrays not yet supported.
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
        
        if self._type_shift == PMATH_TYPE_SHIFT_PACKED_ARRAY:
            if index == 0:
                return ExprVal.get_builtin_symbol('System`List')
            return ExprVal(None)
        
        return ExprVal(None)
    
    def get_debug_info(self):
        if self.get_refcount() <= 0:
            return ExprVal(None)
        
        if self._type_shift in [PMATH_TYPE_SHIFT_EXPRESSION_GENERAL, PMATH_TYPE_SHIFT_EXPRESSION_GENERAL_PART]:
            expr_data = self.dereference().cast(gdb.lookup_type('struct _pmath_expr_t'))
            if expr_data.type.has_key('debug_ptr'):
                return ExprVal.from_pointer(expr_data['debug_ptr'])
                
        return ExprVal(None)

    def get_string_data(self, errorval = u''):
        if self._tag == PMATH_TAG_STR0:
            return u''

        if self._tag == PMATH_TAG_STR1:
            return unichr(self._val['s']['u']['as_chars'][0])

        if self._tag == PMATH_TAG_STR2:
            return unichr(self._val['s']['u']['as_chars'][0]) + unichr(self._val['s']['u']['as_chars'][1])

        if self._type_shift == PMATH_TYPE_SHIFT_BIGSTRING:
            if self.get_refcount() < 1:
                return errorval
            
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
                chars_ptr = chars_ptr.cast(gdb.lookup_type('uint16_t').pointer())
            else:
                if buffer_data['length'] < cap_or_start + length:
                    return errorval
                chars_ptr = string_data['buffer'].cast(gdb.lookup_type('void').pointer()) + ExprVal.string_header_size()
                chars_ptr = chars_ptr.cast(gdb.lookup_type('uint16_t').pointer())
                chars_ptr = chars_ptr + cap_or_start
            return u''.join([unichr(int(chars_ptr[i])) for i in range(length)])

        return errorval
    
    def get_pack_array_element_type(self):
        if not self.is_packed_array():
            return None
        
        packed_array_data = self.dereference().cast(gdb.lookup_type('struct _pmath_packed_array_t'))
        return int(packed_array_data['element_type'])

    def get_pack_array_sizes(self):
        if not self.is_packed_array():
            return []
        
        packed_array_data = self.dereference().cast(gdb.lookup_type('struct _pmath_packed_array_t'))
        length            = int(packed_array_data['dimensions'])
        sizes_and_steps   = packed_array_data['sizes_and_steps']
        return [long(sizes_and_steps[i]) for i in range(length)]

    def get_symbol_name(self):
        if not self.is_symbol():
            return u''
        
        symbol_data = self.dereference().cast(gdb.lookup_type('struct _pmath_symbol_t'))
        return ExprVal(symbol_data['name']).get_string_data()
    
    def get_multirule_pattern(self):
        if not self.is_multirule():
            return ExprVal(None)
        
        if self.get_refcount() <= 0:
            return ExprVal(None)
        
        multirule_data = self.dereference().cast(gdb.lookup_type('struct _pmath_multirule_t'))
        
        return ExprVal(multirule_data['pattern']['_data'])
        
    def get_multirule_body(self):
        if not self.is_multirule():
            return ExprVal(None)
        
        if self.get_refcount() <= 0:
            return ExprVal(None)
        
        multirule_data = self.dereference().cast(gdb.lookup_type('struct _pmath_multirule_t'))
        
        return ExprVal(multirule_data['body']['_data'])
        
    def get_multirule_next(self):
        if not self.is_multirule():
            return ExprVal(None)
        
        if self.get_refcount() <= 0:
            return ExprVal(None)
        
        multirule_data = self.dereference().cast(gdb.lookup_type('struct _pmath_multirule_t'))
        
        return ExprVal(multirule_data['next']['_data'])
    
    def get_custom_data_and_destructor(self):
        if not self.is_custom():
            return None
        
        if self.get_refcount() <= 0:
            return None
        
        try:
            custom_data = self.dereference().cast(gdb.lookup_type('struct _pmath_custom_t'))
            return (custom_data['data'], custom_data['destructor'])
        except gdb.error:
            return None
    
    
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
        
        if self.is_packed_array():
            elem_type = self.get_pack_array_element_type()
            
            if elem_type == PMATH_PACKED_DOUBLE:
                elem_type_string = 'Real'
            elif elem_type == PMATH_PACKED_INT32:
                elem_type_string = 'Integer'
            else:
                elem_type_string = 'Undefined'
            
            sizes = self.get_pack_array_sizes()
            dim_string = ','.join([s.__str__() for s in sizes])
            
            f.write('PackedArray({0}, <<{1}>>)'.format(elem_type_string, dim_string))
            return
        
        if self.is_expr():
            if max_recursion <= 0:
                f.write('...')
                return
            head = self.get_expr_item(0)
            
            length = self.get_expr_length()
            head.write_to_file(f, max_recursion - 1, max_arg_count)
            
            debug_info = self.get_debug_info()
            if debug_info.is_pmath() and not debug_info.is_null():
                #f.write('\xA8') # DIAERESIS
                f.write('/*D*/')
            
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
        
        if self.is_custom():
            dd = self.get_custom_data_and_destructor()
            if dd != None:
                f.write('[custom {0} for {1}]'.format(dd[0], dd[1]))
                return
        
        if self.is_pointer():
            if self.get_pointer() == 0:
                f.write('/\\/')
                return
            
            if ExprVal._type_shift_names.has_key(self._type_shift):
                typename = ExprVal._type_shift_names[self._type_shift]
            else:
                typename = 'type {0}'.format(self._type_shift)
            
            f.write('[{0} at {1}]'.format(typename, self.get_pointer()))
            return
           
        if ExprVal._tag_names.has_key(self._tag):
            tagname = ExprVal._tag_names[self._tag]
        else:
            tagname = 'tag {0}:'.format(self._tag)
        
        f.write('[{0} {1}]'.format(tagname, self._val['s']['u']['as_int32']))
    
class ExprFormatting:
    @static
    def expr_to_file_location(r):
        if r.is_int32():
            return str(line.get_int32())
        elif r.is_expr_of('System`List', 2):
            line = r.get_expr_item(1)
            col  = r.get_expr_item(2)
            
            if line.is_int32() and col.is_int32():
                return '{0}:{1}'.format(line.get_int32(), col.get_int32())
        
        return r.to_string()
        
    @static
    def range_to_file_location(r):
        if r.is_expr_of('System`Range', 2):
            start = r.get_expr_item(1)
            end   = r.get_expr_item(2)
            
            return '{0}..{1}'.format(
                ExprFormatting.expr_to_file_location(start), 
                ExprFormatting.expr_to_file_location(end))
        return r.expr_to_file_location()
    
    @static
    def debug_source_info_to_pair(src):
        if src.is_expr_of('Developer`DebugInfoSource', 2):
            name = src.get_expr_item(1)
            pos  = src.get_expr_item(2)
            
            if name.is_string():
                name = name.get_string_data().replace('\\', '/')
            else:
                name = name.to_string()
            
            pos = ExprFormatting.range_to_file_location(pos)
            return (name, pos)
            
        if src.is_pmath() and not src.is_null():
            return (src.to_string(), None)
            
        return (None, None)
        
    @static
    def debug_source_info_to_string(src):
        pair = ExprFormatting.debug_source_info_to_pair(src)
        if pair[1] == None:
            return pair[0]
        return pair[0].split('/')[-1] + ' (' + pair[1] + ') = {file = ' + pair[0] + ', location = ' + pair[1] + '}'
        