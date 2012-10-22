#ifndef __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__X86_64_H__
#define __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__X86_64_H__

#warning x86_64.h is DEPRECATED. Upgrade to GCC 4.0.0+ to use builtins

/* __inline asm + PIC:
  [1] http://sam.zoy.org/blog/2007-04-13-shlib-with-non-pic-code-have-__inline-assembly-and-pic-mix-well
 */


PMATH_FORCE_INLINE
void pmath_atomic_barrier(void){
  __asm __volatile("mfence":::"memory");
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_read_aquire(pmath_atomic_t *atom){
  intptr_t data = atom->_data;
  pmath_atomic_barrier(); // all reads or writes below this line keep below it (pmath_atomic_barrier() is even a full barrier)
  return data;
}


PMATH_FORCE_INLINE
void pmath_atomic_write_release(pmath_atomic_t *atom, intptr_t value){
  pmath_atomic_barrier(); // all reads or writes above this line keep above it (pmath_atomic_barrier() is even a full barrier)
  atom->_data = value;
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_add(pmath_atomic_t *atom, intptr_t delta){
  intptr_t result;

  // add or xadd?
  __asm __volatile(
    "lock; xaddq %0,%1"
    : "=r"(result), "=m"(atom->_data)
    : "0"(delta),   "m" (atom->_data)
    : "memory"
  );

  return result;
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_set(pmath_atomic_t *atom, intptr_t new_value){
  intptr_t result;

/* I read somewhere that xchg does not need a lock prefix. is that true? */
  __asm __volatile(
    "lock; xchgq %0,%1"
    : "=r"(result),   "=m"(atom->_data)
    : "0"(new_value), "m" (atom->_data)
    : "memory"
  );

  return result;
}


PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_compare_and_set(pmath_atomic_t *atom, intptr_t old_value, intptr_t new_value){
  intptr_t result;

  __asm __volatile(
    "lock; cmpxchgq %2,%1"
    : "=a"(result), "=m"(atom->_data)
    : "q"(new_value), "0"(old_value)
    : "memory", "cc"
  );

  return result;
}


PMATH_FORCE_INLINE
pmath_bool_t pmath_atomic_compare_and_set(pmath_atomic_t *atom, intptr_t old_value, intptr_t new_value){
  char result;

  __asm __volatile(
    "lock; cmpxchgq %2,%1 \n\t"
    "sete %0"
    : "=a"(result), "=m"(atom->_data)
    : "q"(new_value), "0"(old_value)
    : "memory", "cc"
  );

  return (pmath_bool_t)result;
}


PMATH_FORCE_INLINE
pmath_bool_t pmath_atomic_compare_and_set_2(
  pmath_atomic2_t *atom,
  intptr_t old_value_fst,
  intptr_t old_value_snd,
  intptr_t new_value_fst,
  intptr_t new_value_snd
){
  char result;

  // cmpxchg8b mem64
  //
  // -compares RDX:RAX to mem128
  //  if both equal, loads RCX:RBX into mem128
  //  otherwise mem128 is loaded into RDX:RAX
  //
  // -sets or clears the zero (=equal) flag


  // [1] Says we cannot use %%ebx in PIC code.
  #if defined(__pic__) || defined(__PIC__)
    uintptr_t rbx_value;
    
    __asm __volatile(
      "movq %%rbx, %3       \n\t" // rbx_value = %rbx
      "movq %5, %%rbx       \n\t" // %rbx = new_value_fst
      "lock; cmpxchg16b %4  \n\t"
      "sete %%cl            \n\t"
      "movq %3, %%rbx       \n\t" // %rbx = rbx_value
      
      : "=c" (result),
        "=a" (old_value_fst),
        "=d" (old_value_snd),
        "=g" (rbx_value)     // %3
        
      : "m" (*atom),         // %4
        "m" (new_value_fst), // %5
        "0" (new_value_snd), // =c
        "1" (old_value_fst), // =a
        "2" (old_value_snd)  // =d
         
      : "memory", "cc"
    );
  #else
  // we can use %%ebx in non-PIC code
    __asm __volatile(
      "lock; cmpxchg16b %3 \n\t"
      "sete %%cl \n\t"
      
      : "=c" (result),
        "=a" (old_value_fst),
        "=d" (old_value_snd)
        
      : "m" (*atom),         // %3
        "b" (new_value_fst),
        "0" (new_value_snd), // =c
        "1" (old_value_fst), // =a
        "2" (old_value_snd)  // =d
        
      : "memory", "cc"
    );
  #endif

  return (pmath_bool_t)result;
}

PMATH_FORCE_INLINE
pmath_bool_t pmath_atomic_have_cas2(void){
  uintptr_t ecx;
  
  __asm __volatile(
    "pushq %%rbx     \n\t"
    "pushq %%rdx     \n\t"
    "cpuid           \n\t"
    "popq %%rdx      \n\t"
    "popq %%rbx      \n\t"
  : "=c"(ecx)
  : "a"(1));
  
  return (ecx & (1 << 13)) != 0; // CMPXCHG16B support
}
 

//#undef pmath_atomic_loop_nop
//#define pmath_atomic_loop_nop()  __asm __volatile("rep; nop"::)


PMATH_FORCE_INLINE
void pmath_atomic_lock(pmath_atomic_t *atom){
  int cnt = PMATH_ATOMIC_FASTLOOP_COUNT;
  
  while(cnt > 0 && atom->_data != 0){
    --cnt;
  }
  
  if(atom->_data != 0){
    pmath_atomic_loop_yield();
  }
  
  while(0 != pmath_atomic_fetch_set(atom, 1)){
    pmath_atomic_loop_nop();
    pmath_atomic_barrier();
  }
  pmath_atomic_barrier();
}


PMATH_FORCE_INLINE
void pmath_atomic_unlock(pmath_atomic_t *atom){
  atom->_data = 0;
  pmath_atomic_barrier();
}


#define HAVE_ATOMIC_OPS

#endif /* __PMATH_UTIL__CONCURRENCY__ATOMIC__GCC__X86_64_H__ */
