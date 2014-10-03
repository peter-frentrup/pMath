#include <pmath-util/stacks-private.h>

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/memory.h>

#include <string.h>


static pmath_bool_t have_cas2;

static struct _pmath_stack_t *get_stack_ptr(pmath_stack_t stack) {
//  if(((size_t)stack & 0xF) == 0)
//    return (size_t)stack;

  // Round to next multiple of 16
  return (void*)(((((size_t)stack) + 15) / 16) * 16);
}


PMATH_API
pmath_stack_t pmath_stack_new(void) {
// TODO: Need 16 byte aligned pointer on x64 for CMPXCHG16B but pmath_mem_alloc only gives 8 byte (on my machine...)
// Use memalign via some new pmath_mem_aligned_alloc()
  pmath_stack_t stack = pmath_mem_alloc(sizeof(struct _pmath_unaligned_stack_t));
                        
  if(!stack)
    return NULL;
    
  memset(stack, 0, sizeof(*stack));
  pmath_atomic_barrier();
  return stack;
}

PMATH_API
void pmath_stack_free(pmath_stack_t stack) {
  pmath_mem_free(stack);
}

static void  (*stack_push)(pmath_stack_t, void*);
static void *(*stack_pop)( pmath_stack_t);

//{ threadsafe push/pop with CAS and CAS2 (lockfree) ...
static void lockfree_push(pmath_stack_t stack, void *item) {
  struct _pmath_stack_t *stack_ptr = get_stack_ptr(stack);
  
  assert(have_cas2);
  do {
    ((struct _pmath_stack_item_t*)item)->next = stack_ptr->u.s.top;
  } while(!pmath_atomic_compare_and_set(
            (pmath_atomic_t*)&stack_ptr->u.s.top,
            (intptr_t)((struct _pmath_stack_item_t*)item)->next,
            (intptr_t)item));
}

static void *lockfree_pop(pmath_stack_t stack) {
  struct _pmath_stack_t *stack_ptr = get_stack_ptr(stack);
  struct _pmath_stack_item_t *head;
  struct _pmath_stack_item_t *next;
  intptr_t oc;
  
  assert(have_cas2);
  
  do {
    head = stack_ptr->u.s.top;
    oc   = stack_ptr->u.s.operation_counter_or_spinlock;
    if(!head)
      return NULL;
    next = head->next; // What is if we already freed head?
  } while(!pmath_atomic_compare_and_set_2(
            &stack_ptr->u.as_atomic2,
            (intptr_t)head, oc,
            (intptr_t)next, oc + 1));
            
  return head;
}
//}

//{ threadsafe push/pop through locks ...
static void locking_push(pmath_stack_t stack, void *item) {
  struct _pmath_stack_t *stack_ptr = get_stack_ptr(stack);
  
  pmath_atomic_lock((pmath_atomic_t*)&get_stack_ptr(stack)->u.s.operation_counter_or_spinlock);
  
  ((struct _pmath_stack_item_t*)item)->next = stack_ptr->u.s.top;
  stack_ptr->u.s.top = item;
  
  pmath_atomic_unlock((pmath_atomic_t*)&stack_ptr->u.s.operation_counter_or_spinlock);
}

static void *locking_pop(pmath_stack_t stack) {
  struct _pmath_stack_t *stack_ptr = get_stack_ptr(stack);
  void *head;
  
  pmath_atomic_lock((pmath_atomic_t*)&stack_ptr->u.s.operation_counter_or_spinlock);
  
  head = stack_ptr->u.s.top;
  if(head)
    stack_ptr->u.s.top = ((struct _pmath_stack_item_t*)head)->next;
    
  pmath_atomic_unlock((pmath_atomic_t*)&stack_ptr->u.s.operation_counter_or_spinlock);
  return head;
}
//}

PMATH_API
PMATH_ATTRIBUTE_NONNULL(1, 2)
void pmath_stack_push(pmath_stack_t stack, void *item) {
  assert(stack != NULL);
  assert(item != NULL);
  
  stack_push(stack, item);
}

PMATH_API
void *pmath_stack_pop(pmath_stack_t stack) {
  assert(stack != NULL);
  
  return stack_pop(stack);
}

PMATH_PRIVATE
pmath_bool_t _pmath_stacks_init(void) {
  have_cas2 = pmath_atomic_have_cas2();
  
  if(have_cas2) {
    stack_push = lockfree_push;
    stack_pop  = lockfree_pop;
  }
  else {
    stack_push = locking_push;
    stack_pop  = locking_pop;
  }
  
  return TRUE;
}
