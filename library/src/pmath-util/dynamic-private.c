#include <pmath-util/dynamic-private.h>
#include <pmath-util/dynamic.h>

#include <pmath-core/numbers.h>
#include <pmath-core/symbols-private.h>

#include <pmath-util/concurrency/atomic-private.h>
#include <pmath-util/concurrency/threadmsg.h>
#include <pmath-util/emit-and-gather.h>
#include <pmath-util/evaluation.h>
#include <pmath-util/hash/hashtables-private.h>
#include <pmath-util/memory.h>

#include <pmath-builtins/all-symbols-private.h>


struct symbol_list_t {
  struct symbol_list_t *next;
  pmath_symbol_t        symbol;
};

struct id_list_t {
  struct id_list_t *next;
  intptr_t          id;
};

struct symbol2ids_t {
  pmath_symbol_t    symbol;
  
  struct id_list_t *ids;
};

struct id2symbols_t {
  intptr_t              id;
  
  struct symbol_list_t *symbols;
  double                first_eval_time;
};

extern pmath_symbol_t pmath_Internal_DynamicUpdated;

static void id2symbols_destructor(void *p) {
  if(p) {
    struct id2symbols_t *i2s = (struct id2symbols_t*)p;
    struct symbol_list_t *symbols;
    
    symbols = i2s->symbols;
    while(symbols) {
      struct symbol_list_t *next = symbols->next;
      
      pmath_unref(symbols->symbol);
      pmath_mem_free(symbols);
      
      symbols = next;
    }
    
    pmath_mem_free(i2s);
  }
}

static void symbol2ids_destructor(void *p) {
  if(p) {
    struct symbol2ids_t *s2i = (struct symbol2ids_t*)p;
    struct id_list_t *ids;
    
    pmath_unref(s2i->symbol);
    
    ids = s2i->ids;
    while(ids) {
      struct id_list_t *next = ids->next;
      
      pmath_mem_free(ids);
      
      ids = next;
    }
    
    pmath_mem_free(s2i);
  }
}

static unsigned int id2symbols_entry_hash(void *e) {
  struct id2symbols_t *entry = e;
  return _pmath_hash_pointer((void*)entry->id);
}

static unsigned int symbol2ids_entry_hash(void *e) {
  struct symbol2ids_t *entry = e;
  return _pmath_hash_pointer(PMATH_AS_PTR(entry->symbol));
}

static pmath_bool_t id2symbols_entry_keys_equal(void *e1, void *e2) {
  struct id2symbols_t *entry1 = e1;
  struct id2symbols_t *entry2 = e2;
  return entry1->id == entry2->id;
}

static pmath_bool_t symbol2ids_entry_keys_equal(void *e1, void *e2) {
  struct symbol2ids_t *entry1 = e1;
  struct symbol2ids_t *entry2 = e2;
  return pmath_same(entry1->symbol, entry2->symbol);
}

static unsigned int symbol2ids_key_hash(void *key) {
  return _pmath_hash_pointer(PMATH_AS_PTR(*(pmath_t*)key));
}

static pmath_bool_t id2symbols_entry_equals_key(void *e, void *key) {
  struct id2symbols_t *entry = e;
  return entry->id == (intptr_t)key;
}

static pmath_bool_t symbol2ids_entry_equals_key(void *e, void *key) {
  struct symbol2ids_t *entry = e;
  return pmath_same(entry->symbol, *(pmath_t*)key);
}

static pmath_ht_class_t id2symbols_class = {
  id2symbols_destructor,
  id2symbols_entry_hash,
  id2symbols_entry_keys_equal,
  _pmath_hash_pointer,
  id2symbols_entry_equals_key
};
static pmath_ht_class_t symbol2ids_class = {
  symbol2ids_destructor,
  symbol2ids_entry_hash,
  symbol2ids_entry_keys_equal,
  symbol2ids_key_hash,
  symbol2ids_entry_equals_key
};

static pmath_atomic_t id2symbols; // pmath_hashtable_t
static pmath_atomic_t symbol2ids; // pmath_hashtable_t

static void lock_tables(pmath_hashtable_t *i2s, pmath_hashtable_t *s2i) {
  *i2s = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&id2symbols);
  *s2i = (pmath_hashtable_t)_pmath_atomic_lock_ptr(&symbol2ids);
}

static void unlock_tables(pmath_hashtable_t i2s, pmath_hashtable_t s2i) {
  _pmath_atomic_unlock_ptr(&symbol2ids, s2i);
  _pmath_atomic_unlock_ptr(&id2symbols, i2s);
}

/*============================================================================*/

/**
  symbol2ids[symbol]->ids will contain id
  id2symbols[id]->symbols will contain symbol
 */
PMATH_PRIVATE void _pmath_dynamic_bind(pmath_symbol_t symbol, intptr_t id) {
  pmath_hashtable_t i2s_table;
  pmath_hashtable_t s2i_table;
  
  struct id_list_t     *idlist;
  struct symbol_list_t *symlist;
  
  if(id == 0)
    return;
    
  idlist  = pmath_mem_alloc(sizeof(struct id_list_t));
  symlist = pmath_mem_alloc(sizeof(struct symbol_list_t));
  
  if(!idlist || !symlist) {
    pmath_mem_free(idlist);
    pmath_mem_free(symlist);
    
    return;
  }
  
  lock_tables(&i2s_table, &s2i_table);
  {
    struct symbol2ids_t *s2i_entry;
    struct id2symbols_t *i2s_entry;
    
    i2s_entry = pmath_ht_search(i2s_table, (void*)id);
    s2i_entry = pmath_ht_search(s2i_table, &symbol);
    
    if(s2i_entry) {
      idlist->id   = id;
      idlist->next = s2i_entry->ids;
      
      s2i_entry->ids = idlist;
      
      idlist = NULL;
    }
    else {
      s2i_entry = pmath_mem_alloc(sizeof(struct symbol2ids_t));
      
      if(s2i_entry) {
        idlist->id   = id;
        idlist->next = NULL;
        
        s2i_entry->symbol = pmath_ref(symbol);
        s2i_entry->ids    = idlist;
        
        idlist = NULL;
        
        s2i_entry = pmath_ht_insert(s2i_table, s2i_entry);
        symbol2ids_destructor(s2i_entry);
      }
    }
    
    if(i2s_entry) {
      symlist->symbol = pmath_ref(symbol);
      symlist->next   = i2s_entry->symbols;
      
      i2s_entry->symbols = symlist;
      
      symlist = NULL;
    }
    else {
      i2s_entry = pmath_mem_alloc(sizeof(struct id2symbols_t));
      
      if(i2s_entry) {
        symlist->symbol = pmath_ref(symbol);
        symlist->next   = NULL;
        
        i2s_entry->id              = id;
        i2s_entry->symbols         = symlist;
        i2s_entry->first_eval_time = pmath_tickcount();
        
        symlist = NULL;
        
        i2s_entry = pmath_ht_insert(i2s_table, i2s_entry);
        symbol2ids_destructor(i2s_entry);
      }
    }
  }
  unlock_tables(i2s_table, s2i_table);
  
  pmath_mem_free(idlist);
  pmath_mem_free(symlist);
}

PMATH_PRIVATE pmath_bool_t _pmath_dynamic_remove(intptr_t id) {
  pmath_hashtable_t i2s_table;
  pmath_hashtable_t s2i_table;
  struct id2symbols_t *i2s_entry = NULL;
  
  lock_tables(&i2s_table, &s2i_table);
  {
    i2s_entry = pmath_ht_remove(i2s_table, (void*)id);
    
    if(i2s_entry) {
      struct symbol_list_t *symbols = i2s_entry->symbols;
      
      while(symbols) {
        struct symbol2ids_t *s2i_entry;
        
        s2i_entry = pmath_ht_search(s2i_table, &symbols->symbol);
        
        if(s2i_entry) {
          struct id_list_t *ids, **prev_id;
          
          prev_id = &s2i_entry->ids;
          ids = *prev_id;
          while(ids) {
            if(ids->id == id) {
              *prev_id = ids->next;
              break;
            }
            
            prev_id = &ids->next;
            ids = *prev_id;
          }
          
          pmath_mem_free(ids);
          if(s2i_entry->ids == NULL) {
            _pmath_symbol_lost_dynamic_tracker(symbols->symbol, id, 0);
            symbol2ids_destructor(pmath_ht_remove(s2i_table, &symbols->symbol));
          }
          else {
            _pmath_symbol_lost_dynamic_tracker(symbols->symbol, id, s2i_entry->ids->id);
          }
        }
        
        symbols = symbols->next;
      }
      
      id2symbols_destructor(i2s_entry);
    }
  }
  unlock_tables(i2s_table, s2i_table);
  
  return i2s_entry != NULL;
}

PMATH_PRIVATE void _pmath_dynamic_update(pmath_symbol_t symbol) {
  pmath_hashtable_t i2s_table;
  pmath_hashtable_t s2i_table;
  struct symbol2ids_t *s2i_entry = NULL;
  
  lock_tables(&i2s_table, &s2i_table);
  {
    s2i_entry = pmath_ht_remove(s2i_table, &symbol);
  }
  unlock_tables(i2s_table, s2i_table);
  
  if(s2i_entry) {
    struct id_list_t *ids = s2i_entry->ids;
    
    pmath_gather_begin(PMATH_NULL);
    
    while(ids) {
      if(_pmath_dynamic_remove(ids->id)) {
        pmath_emit(pmath_integer_new_siptr(ids->id), PMATH_NULL);
      }
      
      ids = ids->next;
    }
    
    symbol2ids_destructor(s2i_entry);
    
    pmath_unref(
      pmath_evaluate(
        pmath_expr_set_item(
          pmath_gather_end(),
          0, pmath_ref(pmath_Internal_DynamicUpdated))));
  }
}

PMATH_PRIVATE double _pmath_dynamic_first_eval(intptr_t id) {
  pmath_hashtable_t i2s_table;
  pmath_hashtable_t s2i_table;
  double t = 0;
  
  lock_tables(&i2s_table, &s2i_table);
  {
    struct id2symbols_t *i2s_entry;
    
    i2s_entry = pmath_ht_search(i2s_table, (void*)id);
    
    if(i2s_entry) {
      t = i2s_entry->first_eval_time;
    }
    else {
      i2s_entry = pmath_mem_alloc(sizeof(struct id2symbols_t));
      
      if(i2s_entry) {
        i2s_entry->id              = id;
        i2s_entry->symbols         = NULL;
        i2s_entry->first_eval_time = t = pmath_tickcount();
        
        i2s_entry = pmath_ht_insert(i2s_table, i2s_entry);
        symbol2ids_destructor(i2s_entry);
      }
    }
  }
  unlock_tables(i2s_table, s2i_table);
  
  return t;
}

/*============================================================================*/

PMATH_PRIVATE pmath_atomic_t _pmath_dynamic_trackers;

PMATH_PRIVATE pmath_bool_t _pmath_dynamic_init(void) {
  pmath_hashtable_t i2s, s2i;
  
  pmath_atomic_write_release(&_pmath_dynamic_trackers, 0);
  
  i2s = pmath_ht_create(&id2symbols_class, 0);
  s2i = pmath_ht_create(&symbol2ids_class, 0);
  
  if(!i2s || !s2i) {
    pmath_ht_destroy(i2s);
    pmath_ht_destroy(s2i);
    return FALSE;
  }
  
  pmath_atomic_write_release(&id2symbols, (intptr_t)i2s);
  pmath_atomic_write_release(&symbol2ids, (intptr_t)s2i);
  
  return TRUE;
}

PMATH_PRIVATE void _pmath_dynamic_done(void) {
  pmath_ht_destroy((pmath_hashtable_t)pmath_atomic_read_aquire(&id2symbols));
  pmath_ht_destroy((pmath_hashtable_t)pmath_atomic_read_aquire(&symbol2ids));
}
