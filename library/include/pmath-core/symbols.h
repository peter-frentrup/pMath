#ifndef __PMATH_CORE__SYMBOLS_H__
#define __PMATH_CORE__SYMBOLS_H__

#include <pmath-core/strings.h>

/**\defgroup symbols Symbols
   \brief Symbol objects in pMath.

  @{
 */

/**\class pmath_symbol_t
   \extends pmath_t
   \brief The Symbol class.

   The pMath language knows symbols as `named entities` that can hold any
   pMath object as a value and have some attributes. Those symbols are objects
   themselves. A symbol name consists of alphanumerical characters (a-z,A-Z,0-9)
   and apostrophes to seperate namespaces. All standard symbols are in the
   "System" namespace (e.g. System`print). All user defined symbols go to
   "Global". Every other module has its own namespace.
   
   Because pmath_symbol_t is derived from pmath_t, you can use strings wherever
   a pmath_t is accepted. E.g. you compare two symbols with pmath_compare() or
   pmath_equals().

   The pmath_type_t of symbols is PMATH_TYPE_SYMBOL.

   \see objects
 */
typedef pmath_t pmath_symbol_t;

/**\brief The (bitset) type of symbol attributes.

   A pMath symbol (here called `sym`) can have one or more of the following
   values (concatenated with "|"):
   <ul>
     <li> \c PMATH_SYMBOL_ATTRIBUTE_PROTECTED \n
       Any assignment to sym will fail.
     
     <li> \c PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST \n
       When evaluating `sym(a,b,...)`, the first argument (a) will not be 
       evaluated automatically.
       
     <li> \c PMATH_SYMBOL_ATTRIBUTE_HOLDREST \n
       When evaluating `sym(a,b,...)` all the arguments b,... will not be 
       evaluated automatically.
     
     <li> \c PMATH_SYMBOL_ATTRIBUTE_HOLDALL \n
       combines HOLDFIRST and HOLDREST.
     
     <li> \c PMATH_SYMBOL_ATTRIBUTE_SYMMETRIC \n
       An expression `sym(a,b,...)` will be sorted automatically and thus 
       sym(a,b) = sym(b,a).
       
     <li> \c PMATH_SYMBOL_ATTRIBUTE_ASSOCIATIVE \n
       An expression `sym(...,sym(a,...,z),...)` will be flattened automatically 
       to sym(...,a,...,z,...).
       
     <li> \c PMATH_SYMBOL_ATTRIBUTE_NHOLDFIRST \n
       The first argument (a) of `sym(a,b,...)` will not be affected by 
       Approximate(sym(a,b,...)).
       
     <li> \c PMATH_SYMBOL_ATTRIBUTE_NHOLDREST \n
       All the argument `b,...` in `sym(a,b,...)` will not be affected by 
       Approximate(sym(a,b,...)).
       
     <li> \c PMATH_SYMBOL_ATTRIBUTE_NHOLDALL \n
       combines NHOLDFIRST and NHOLDREST.
       
     <li> \c PMATH_SYMBOL_ATTRIBUTE_TEMPORARY \n
       The symbol sym will be deleted immediately when it is no longer 
       referenced. It will be freed automatically (but not immediately), when 
       there is no external reference to the symbol (just the symbol's own 
       function definitions/...).
       
     <li> \c PMATH_SYMBOL_ATTRIBUTE_LISTABLE \n
       Any expression sym(...) will be threaded automatically over lists. 
       (e.g. {a,b} + {c,d} becomes {a+c, b+d}).
       
     <li> \c PMATH_SYMBOL_ATTRIBUTE_DEEPHOLDALL The arguments `a,...` in an
       expression `sym(...)(a,...)` will not be evaluated automatically.
       
     <li> \c PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE \n 
       Like HOLDALL, but in an expression `sym(a,b,...)` all arguments 
       (a,b,...) wont be touched even if they have the form `eval(...)`. 
       Additionally, rules for sym defined in one of its arguments 
       (e.g. `a: sym(a):= "hi"`) wont be used.
       
     <li> \c PMATH_SYMBOL_ATTRIBUTE_ONEIDENTITY \n
       Used for pattern matching (in combination with ASSOCIATIVE) to say that
       `sym(x)` matches x. Note that it does not automatically evaluate `sym(x)`
       to x.
       
     <li> \c PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL \n
       The symbol's value is local to the current thread. That means, an assignment to sym in one thread wont affect
       it in another thread.
     
     <li> \c PMATH_SYMBOL_ATTRIBUTE_NUMERICFUNCTION \n
       `sym(x,...)` is numeric if all the arguments are numeric.
     
     <li> \c PMATH_SYMBOL_ATTRIBUTE_READPROTECTED
       `??sym` wont print out the value/function definitions for sym.
     
     <li> \c PMATH_SYMBOL_ATTRIBUTE_SEQUENCEHOLD
       Sequence(...) wont be sliced when it appears as an argument to `sym(...)`
     
     <li> \c PMATH_SYMBOL_ATTRIBUTE_REMOVED
       The symbol was removed, but there are pending references to it.
     
   </ul>
 */
typedef int pmath_symbol_attributes_t;
enum{
  PMATH_SYMBOL_ATTRIBUTE_PROTECTED             = 1 << 0,
  PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST             = 1 << 1,
  PMATH_SYMBOL_ATTRIBUTE_HOLDREST              = 1 << 2,
  PMATH_SYMBOL_ATTRIBUTE_HOLDALL               = PMATH_SYMBOL_ATTRIBUTE_HOLDFIRST | PMATH_SYMBOL_ATTRIBUTE_HOLDREST,
  PMATH_SYMBOL_ATTRIBUTE_SYMMETRIC             = 1 << 3,
  PMATH_SYMBOL_ATTRIBUTE_ASSOCIATIVE           = 1 << 4,
  PMATH_SYMBOL_ATTRIBUTE_NHOLDFIRST            = 1 << 5,
  PMATH_SYMBOL_ATTRIBUTE_NHOLDREST             = 1 << 6,
  PMATH_SYMBOL_ATTRIBUTE_NHOLDALL              = PMATH_SYMBOL_ATTRIBUTE_NHOLDFIRST | PMATH_SYMBOL_ATTRIBUTE_NHOLDREST,
  PMATH_SYMBOL_ATTRIBUTE_TEMPORARY             = 1 << 7,
  PMATH_SYMBOL_ATTRIBUTE_LISTABLE              = 1 << 8,
  PMATH_SYMBOL_ATTRIBUTE_DEEPHOLDALL           = 1 << 9,
  PMATH_SYMBOL_ATTRIBUTE_HOLDALLCOMPLETE       = 1 << 10,
  PMATH_SYMBOL_ATTRIBUTE_ONEIDENTITY           = 1 << 11,
  PMATH_SYMBOL_ATTRIBUTE_THREADLOCAL           = 1 << 12,
  PMATH_SYMBOL_ATTRIBUTE_NUMERICFUNCTION       = 1 << 13,
  PMATH_SYMBOL_ATTRIBUTE_READPROTECTED         = 1 << 14,
  PMATH_SYMBOL_ATTRIBUTE_SEQUENCEHOLD          = 1 << 15,
  PMATH_SYMBOL_ATTRIBUTE_REMOVED               = 1 << 16
};

/**\brief Get a symbol by its fully qualified name.
   \memberof pmath_symbol_t
   \param name The symbol's name including its namespace. It will be freed.
   \param create Whether to create a new symbol, if none was found.
   \return NULL or a symbol called \c name that must be destroyed with
           pmath_unref().
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_symbol_t pmath_symbol_get(
  pmath_string_t  name,
  pmath_bool_t create);

/**\brief Create a new temporary symbol.
   \memberof pmath_symbol_t
   \param name The base name of the temporary symbol. It will be freed.
   \param unique Whether to add a unique number to the symbol name.
   \return A new pMath Symbol. You must destroy it with pmath_unref().
           It has the Temporary attribute.
           
           the name of the returned symbol is of the form name$nnn (or name$ if
           unique is false)
           
           If name already has the form "sym$nnn" or "sym$", the function acts 
           as if name would be simply "sym".
           
           If there already exists a symbol with the generated name, that symbol
           will be returned and its attributes will be set to Temporary before.
  */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_symbol_t pmath_symbol_create_temporary(
  pmath_string_t  name,
  pmath_bool_t unique);

/**\brief Find a symbol in the current namespace search path.
   \memberof pmath_symbol_t
   \param name The symbol's name. It will be freed.
   \param create Whether to create a new symbol, if none was found.
   \return NULL or a symbol called \c name that must be destroyed with
           pmath_unref().
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_symbol_t pmath_symbol_find(
  pmath_string_t  name,
  pmath_bool_t create);

/**\brief Get a symbol's name.
   \memberof pmath_symbol_t
   \param symbol A pMath symbol.
   \return The name of the symbol. You must destroy it with pmath_unref().
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_string_t pmath_symbol_name(pmath_symbol_t symbol);

/**\brief Get a symbol's attributes.
   \memberof pmath_symbol_t
   \param symbol A pMath symbol. It wont be freed.
   \return The symbol's attributes.
 */
PMATH_API 
pmath_symbol_attributes_t pmath_symbol_get_attributes(pmath_symbol_t symbol);

/**\brief Set a symbol's attributes.
   \memberof pmath_symbol_t
   \param symbol A pMath symbol. It wont be freed.
   \param attr The new attributes.
 */
PMATH_API 
void pmath_symbol_set_attributes(
  pmath_symbol_t             symbol,
  pmath_symbol_attributes_t  attr);

/**\brief Get a symbol's value.
   \deprecated
   \memberof pmath_symbol_t
   \param symbol A pMath symbol.
   \return The symbol's value. You must free it with pmath_unref(). Note that
           not every object is evaluatable (e.g. \ref custom ).
 */
PMATH_API 
PMATH_ATTRIBUTE_USE_RESULT
pmath_t pmath_symbol_get_value(pmath_symbol_t symbol);

/**\brief Set a symbol's value.
   \deprecated
   \memberof pmath_symbol_t
   \param symbol A pMath symbol. It wont be freed
   \param value The new value. It will be freed.

   This function ignores the Protected-attribute. You should only use it during
   symbol initialization and/or when you want to store non-evaluatable values
   in a symbol. In all other cases, evaluate an expression with the head
   PMATH_SYMBOL_ASSIGN or PMATH_SYMBOL_ASSIGNDELAYED.
 */
PMATH_API 
void pmath_symbol_set_value(
  pmath_symbol_t symbol,
  pmath_t        value);

/**\brief Execute a function synchronized to a symbol.
   \deprecated
   \memberof pmath_symbol_t
   \param symbol The symbol to lock. It wont be freed.
   \param callback The function to be executed when the symbol is locked.
   \param data A pointer that will be passed to callback.

   \see threads
 */
PMATH_API 
void pmath_symbol_synchronized(
  pmath_symbol_t     symbol,
  pmath_callback_t   callback,
  void              *data);

/**\brief Update a symbol manually.
   \memberof pmath_symbol_t
   \param symbol A pMath symbol. It wont be freed.

   You normally do not have to call this, since every change in a symbol yields
   an update. But there are some situations where you might to update it
   manually. The update mechanism is an optimization. Any expresseion or symbol,
   that is up to date while evaluation, wont be evaluated again. After an
   evaluation, expressions are updated automatically.
 */
PMATH_API 
void pmath_symbol_update(pmath_symbol_t symbol);

/**\brief Remove a symbol completely from the system.
   \param symbol a pMath symbol. It will be freed.
   
   Symbols with attribute protected wont be removed.
   
   This function walks through the internal list of all known symbols and 
   replaces any occurencies with `Symbol("name")`. 
   
   There might be more references (e.g. on the stack or in other thread's local 
   variable tables), so it is possible that the symbol still exists in the
   system. 
   
   Note that all builtin symbols (the PMATH_SYMBOL_XXX) are also referenced in
   a seperate list and so cannot be removed completely from the system. However,
   their appearences in all other places will be removed. So this is a very
   dangerous function. 
 */
PMATH_API
void pmath_symbol_remove(pmath_symbol_t symbol);

/**\brief Iterate through the global symbol table.
   \param old The previous symbol. It will be freed.
   \return The next symbol.
   
   To actually iterate through the whole list, use the following pattern:
   \code
pmath_symbol_t iter = pmath_ref(PMATH_SYMBOL_LIST);
do{
  
  ... loop body here ...
  
  iter = pmath_symbol_iter_next(iter);
}while(iter && iter != PMATH_SYMBOL_LIST);
pmath_unref(iter);
   \endcode
 */
PMATH_API pmath_symbol_t pmath_symbol_iter_next(pmath_symbol_t old);

/** @} */

#endif /* __PMATH_CORE__SYMBOLS_H__ */
