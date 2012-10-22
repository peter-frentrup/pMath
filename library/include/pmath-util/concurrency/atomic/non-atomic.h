#ifndef __PMATH_UTIL__CONCURRENCY__ATOMIC__NON_ATOMIC_H__
#define __PMATH_UTIL__CONCURRENCY__ATOMIC__NON_ATOMIC_H__

#warning Your compiler or system is currently unsupported. Falling back to a non-atomic (and thus NOT THREADSAFE) implementation. 

/**\addtogroup atomic_ops
   @{
 */

#undef PMATH_ATOMIC_FASTLOOP_COUNT
/**\def PMATH_ATOMIC_FASTLOOP_COUNT
   \brief Loop iterations in spinlocks before yielding control.

   If the thread holding the lock sits on another CPU, spinning around a bit
   before pmath_atomic_loop_yield() reduces idle time. But if the thread holding
   the lock lives on the same CPU as the current thread (and thus is interrupted
   by the current thread), spinning elongates the wait time.

  In summary, this should not be a compile time constant as it is now!
 */
#define PMATH_ATOMIC_FASTLOOP_COUNT    (0)


/**\brief Declares a variable with specified alignment.
   \param TYPE The variable type, possibly including the \c volatile modifier.
   \param NAME The variable name.
   \param ALIGNMENT The alignment in bytes
 */
#define PMATH_DECLARE_ALIGNED(TYPE, NAME, ALIGNMENT)  TYPE NAME

/**\brief Atomically read a value with aquire semantics.
 */
PMATH_FORCE_INLINE
intptr_t pmath_atomic_read_aquire(pmath_atomic_t *atom) {
  intptr_t data = atom->_data;
  pmath_atomic_barrier(); // in theory: all reads or writes below this line keep below it (pmath_atomic_barrier() is even a full barrier)
  return data
}

/**\brief Atomically write a value with release semantics.
 */
PMATH_FORCE_INLINE
void pmath_atomic_write_release(pmath_atomic_t *atom, intptr_t value){
  pmath_atomic_barrier(); // in theory: all reads or writes above this line keep above it (pmath_atomic_barrier() is even a full barrier)
  atom->_data = value;
}

/**\brief Add a value to another.
   \param atom An atomic variable.
   \param delta The difference between the new and the old value.
   \return The old value of \c *atom.

   This function increments \c *atom atomically by \c delta. It has full memory
   barrier semantics.
 */
PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_add(
  pmath_atomic_t *atom, 
  intptr_t        delta
) {
  intptr_t result = *atom;
  *atom += delta;
  return result;
}

/**\brief Exchange a value.
   \param atom An atomic variable.
   \param new_value The new value of \c *atom.
   \return The old value of \c *atom.

   This function sets \c *atom to \c new_value and returns the old value
   atomically. It has full memory barrier semantics.
 */
PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_set(
  pmath_atomic_t *atom,
  intptr_t        new_value
) {
  intptr_t result = *atom;
  *atom = new_value;
  return result;
}

/**\brief Exchange a value if it equals another value.
   \param atom An atomic variable.
   \param old_value The comparisor.
   \param new_value The possible new value of \c *atom.
   \return The old value of \c *atom.

   You should use pmath_atomic_compare_and_set() if you don't need the exact old
   value of \c *atom, because this function might be non-existent on some
   systems. This function has aquire barrier semantics.
 */
PMATH_FORCE_INLINE
intptr_t pmath_atomic_fetch_compare_and_set(
  pmath_atomic_t *atom,
  intptr_t        old_value,
  intptr_t        new_value
) {
  if(*atom == old_value) {
    *atom = new_value;
    return old_value;
  }
  return *atom;
}

/**\brief Exchange a value if it equals another value.
   \param atom An atomic variable.
   \param old_value The comparisor.
   \param new_value The possible new value of \c *atom.
   \return Whether the exchange was performed.

   This function compares \c *atom with \c old_value and iff both equal sets
   \c *atom to \c new_value, everything atomically and with aquire barrier
   semantics.
 */
PMATH_FORCE_INLINE
pmath_bool_t pmath_atomic_compare_and_set(
  pmath_atomic_t *atom,
  intptr_t        old_value,
  intptr_t        new_value
) {
  if(*atom == old_value) {
    *atom = new_value;
    return TRUE;
  }
  return FALSE;
}

/**\brief Exchange two values value if they equal another two values.
   \param *atom An atomic variable of size 2 * sizeof(void*).
   \param old_value_fst The first old value.
   \param old_value_snd The second old value.
   \param new_value_fst The possible new value of \c atom[0].
   \param new_value_snd The possible new value of \c atom[1].
   \return Whether the exchange was performed or not.

   This function compares \c old_value_fst with \c atom[0] and \c old_value_snd
   with atom[1].
   If they equal, \c atom[0] is set to \c new_value_fst and \c atom[1] is set to
   \c new_value_snd and TRUE is returned. Otherwise, FALSE will be returned.

   This function has aquire barrier semantics.

   \note This function is not available on all Platforms. You must not call it
         if pmath_atomic_have_cas2() returns FALSE.
 */
PMATH_FORCE_INLINE
pmath_bool_t pmath_atomic_compare_and_set_2(
  pmath_atomic2_t *atom,
  intptr_t         old_value_fst,
  intptr_t         old_value_snd,
  intptr_t         new_value_fst,
  intptr_t         new_value_snd
) {
  if(atom[0] == old_value_fst && atom[1] == old_value_snd) {
    atom[0] = new_value_fst;
    atom[0] = new_value_snd;
    return TRUE;
  }
  return FALSE;
}

/**\brief Check, whether the CPU supports pmath_atomic_compare_and_set_2().
   \return whether pmath_atomic_compare_and_set_2() is supported.

   Note, that a call to pmath_atomic_compare_and_set_2() will crash your
   application on any platform that does not support the operation (e.g.
   pre-Pentiums, early AMD64).
 */
PMATH_FORCE_INLINE
pmath_bool_t pmath_atomic_have_cas2(void) {
  return FALSE;
}

/**\brief Insert an explicit memory barrier.
 */
PMATH_FORCE_INLINE
void pmath_atomic_barrier(void) {
}


/**\brief Try to aquire a lock.
   \param atom The lock. An atomic variable.

   This function implements a spin lock. It has aquire barrier semantics. Use
   it with pmath_atomic_unlock():
   \code
pmath_atomic_t spin = PMATH_ATOMIC_STATIC_INIT;
...
pmath_atomic_lock(&spin)
... critical section ...
pmath_atomic_unlock(&spin);
   \endcode
 */
PMATH_FORCE_INLINE
void pmath_atomic_lock(
  pmath_atomic_t *atom
) {
  *atom = 1;
}

/**\brief Release a previously aquired lock.
   \param atom The lock. An atomic variable.

   \see pmath_atomic_lock
 */
PMATH_FORCE_INLINE
void pmath_atomic_unlock(
  pmath_atomic_t *atom
) {
  *atom = 0;
}

#undef pmath_atomic_loop_yield
/**\brief Yield control to another thread (used in spinlocks).
 */
PMATH_FORCE_INLINE
void pmath_atomic_loop_yield(void) {
}

#undef pmath_atomic_loop_nop
/**\brief A no-operation or short wait for use in spin locks.
 */
PMATH_FORCE_INLINE
void pmath_atomic_loop_nop(void) {
}

/** @} */

#endif /* __PMATH_UTIL__CONCURRENCY__ATOMIC__NON_ATOMIC_H__ */
