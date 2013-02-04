import sys
import gdb
from pmath_gdb.expr import ExprVal

class static:
    "Creates a 'static' method"
    def __init__(self, function):
        self.__call__ = function

class ManagedStackFrame:
    def __init__(self, stack_info, thread):
        self.stack_info = stack_info
        self.head       = ExprVal(stack_info['value'])
        self.thread     = thread
        self.native_frame = None
        
    def __str__(self):
        return 'thread {0:<10} in {1}'.format(self.thread.address, self.head.to_string(max_recursion=1))

class ManagedStack:
    def __init__(self):
        self.frames = []
        
    @static
    def search(max_count = 20):
        try:
            thread = gdb.parse_and_eval('pmath_thread_get_current()').dereference()
            stack_info = thread['stack_info'].dereference()
        except gdb.MemoryError:
            return ManagedStack()
        
        native_frame = gdb.newest_frame()
        result = ManagedStack()
        while max_count > 0:
            max_count-= 1
            try:
                mf = ManagedStackFrame(stack_info, thread)
                while native_frame != None:
                    current = native_frame
                    native_frame = native_frame.older()
                    try:
                        if current.read_var('stack_frame').address == stack_info.address:
                            mf.native_frame = current
                            break
                    except ValueError:
                        pass
                result.frames.append(mf)
                stack_info = stack_info['next'].dereference()
            except gdb.MemoryError:
                try:
                    thread = thread['parent'].dereference()
                    stack_info = thread['stack_info'].dereference()
                except gdb.MemoryError:
                    break
        return result
        
    def __str__(self):
        return ''.join(['#{0:<3}{1}\n'.format(i, self.frames[i]) for i in range(len(self.frames))])
