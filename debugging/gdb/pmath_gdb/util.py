import sys
import gdb
from pmath_gdb.expr import ExprVal, ExprFormatting

class ManagedStackFrame:
    def __init__(self, stack_info, thread):
        self.stack_info = stack_info
        self.head       = ExprVal(stack_info['head'])
        self.debug_metadata = ExprVal(stack_info['debug_metadata'])
        self.thread     = thread
        self.native_frame = None
        
    def __str__(self):
        s = 'thread {0:<10} in {1}'.format(self.thread.address, self.head.to_string(max_recursion=1))
        
        file_pos = ExprFormatting.debug_source_info_to_pair(self.debug_metadata)
        
        if file_pos[0] != None:
            s+= ' from ' + file_pos[0]
            
            if file_pos[1] != None:
                s+= ' line ' + file_pos[1]
            
        return s

class ManagedStack:
    def __init__(self):
        self.frames = []
        
    @staticmethod
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
