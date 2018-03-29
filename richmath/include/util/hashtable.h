#ifndef __UTIL__HASHTABLE_H__
#define __UTIL__HASHTABLE_H__

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <utility>

#include <util/base.h>

#ifndef NDEBUG
#  define RICHMATH_DEBUG_HASHTABLES

#  define HASHTABLE_ASSERT(a) \
  do{if(!(a)){ \
      richmath::assert_failed(); \
      assert(a); \
    }}while(0)
#else
#  define HASHTABLE_ASSERT(a)  ((void)0)
#endif



namespace richmath {
  template<class T>
  unsigned int object_hash(const T &t) {
    return t.hash();
  }
  
  template<typename T>
  struct cast_hash_impl {
    static unsigned int hash(const T &t) {
      return (unsigned int)t;
    }
  };
  
  template<typename TP>
  struct cast_hash_impl<TP *> {
    static unsigned int hash(TP *const &t) {
#if PMATH_BITSIZE == 32
      return (unsigned int)(uintptr_t)t;
#else
      uintptr_t tmp = (uintptr_t)t;
      return (unsigned int)((tmp & 0xFFFFFFFFU) ^ (tmp >> 32));
#endif
    }
  };
  
  template<typename T>
  unsigned int cast_hash(const T &t) {
    return cast_hash_impl<T>::hash(t);
  }
  
  class Void { /* Note that this takes up one byte! */
  };
  
  template<typename K, typename V>
  class Entry {
    public:
      Entry(const K &k, const V &v): key(k), value(v) {}
      Entry(const K &k, V &&v): key(k), value(std::move(v)) {}
      
      Entry(const Entry<K, V> &src) = delete;
      const Entry &operator=(const Entry<K, V> &src) = delete;
      
      const K key;
      V value;
  };
  
  template<typename K>
  class Entry<K, Void> {
    public:
      explicit Entry(const K &k): key(k) {}
      Entry(const K &k, const Void &v): key(k) {}
      
      const K key;
      static Void value;
  };
  
  template<typename K> Void Entry<K, Void>::value;
  
  template < typename K,
             typename V,
             unsigned int (*hash_function)(const K &key) = object_hash >
  class Hashtable: public Base {
    private:
      typedef Hashtable<K, V, hash_function> self_t;
      typedef Entry<K, V>                    entry_t;
      
      static const unsigned int MINSIZE = 8; // power of 2, >= 2
      static Entry<K, V> *Deleted() { return (Entry<K, V> *)(-(size_t)1); }
      
      static bool is_used(const Entry<K, V> *e) {
        return ((size_t)e) + 1 > 1;
      }
      
      static bool is_deleted(const Entry<K, V> *e) {
        return ((size_t)e) + 1 == 0;
      }
      
    private:
      unsigned int   nonnull_count; // used_count <= nonnull_count < capacity
      unsigned int   used_count;
      unsigned int   capacity;      // power of 2, >= MINSIZE
      Entry<K, V>   **table;
      Entry<K, V>    *small_table[MINSIZE];
#ifdef RICHMATH_DEBUG_HASHTABLES
      unsigned _debug_change_canary;
      unsigned _debug_num_iterators;
      
      void do_change() { 
        HASHTABLE_ASSERT((_debug_num_iterators == 0) && "cannot mutate hashtable while there are still iterators to it");
        ++_debug_change_canary; 
      }
      void added_iterator() { ++_debug_num_iterators; }
      void removed_iterator() {
        HASHTABLE_ASSERT(_debug_num_iterators > 0);
        --_debug_num_iterators; 
      }
#else
      void do_change() { }
      void added_iterator() { }
      void removed_iterator() { }
#endif
      
    public:
      V default_value;
      
      template<class E>
      class Iterator {
          friend self_t;
        private:
          Iterator(E **entries, unsigned int unused_count, self_t &owning_table)
            : _entries(entries), _unused_count(unused_count)
#ifdef RICHMATH_DEBUG_HASHTABLES
            , _owning_table(owning_table),
              _debug_change_canary(owning_table._debug_change_canary)
#endif
          {
            owning_table.added_iterator();
          }
          
        public:
          ~Iterator() {
#ifdef RICHMATH_DEBUG_HASHTABLES
            _owning_table.removed_iterator();
#endif
          }
          
          bool operator!=(const Iterator &other) const {
            HASHTABLE_ASSERT(_debug_change_canary == _owning_table._debug_change_canary);
            return _unused_count != other._unused_count;
          }
          const E &operator*() const {
            HASHTABLE_ASSERT(is_used(*_entries));
            HASHTABLE_ASSERT(_debug_change_canary == _owning_table._debug_change_canary);
            return **_entries;
          }
          E &operator*() {
            HASHTABLE_ASSERT(is_used(*_entries));
            HASHTABLE_ASSERT(_debug_change_canary == _owning_table._debug_change_canary);
            return **_entries;
          }
          const Iterator &operator++() {
            HASHTABLE_ASSERT(_debug_change_canary == _owning_table._debug_change_canary);
            while(_unused_count > 0) {
              ++_entries;
              if(is_used(*_entries)) {
                --_unused_count;
                break;
              }
            }
            return *this;
          }
          
        private:
          E **_entries;
          unsigned int _unused_count;
#ifdef RICHMATH_DEBUG_HASHTABLES
          self_t &_owning_table;
          unsigned _debug_change_canary;
#endif
      };
      
      typedef Iterator<Entry<K, V>> iterator_t;
      typedef Iterator<const Entry<K, V>> const_iterator_t;
      
    private:
      unsigned int lookup(const K &key) const {
        unsigned int freeslot = -(unsigned int)1;
        unsigned int h        = hash_function(key);
        unsigned int index    = h & (capacity - 1);
        
        for(;;) {
          if(!table[index]) {
            if(freeslot == -(unsigned int)1)
              return index;
            return freeslot;
          }
          
          if(is_deleted(table[index])) {
            if(freeslot == -(unsigned int)1)
              freeslot = index;
          }
          else if(table[index]->key == key)
            return index;
            
          index = (5 * index + 1 + h) & (capacity - 1);
          h >>= 5;
        }
        
        assert(0);
        //return 0;
      }
      
      void resize(unsigned int minused) {
        unsigned int newsize;
        
        for(newsize = MINSIZE; newsize <= minused && 0 < (int)newsize; newsize <<= 1) {
        }
        
        if(newsize == capacity)
          return;
          
        if((int)newsize <= 0)
          abort();
        //throw std::bad_alloc();
        
        capacity = newsize;
        Entry<K, V> **oldtable = table;
        if(capacity == MINSIZE)
          table = small_table;
        else
          table = new Entry<K, V> *[capacity];
          
        HASHTABLE_ASSERT(table != oldtable);
        
        memset(table, 0, newsize * sizeof(Entry<K, V> *));
        
        unsigned int i = used_count;
        for(Entry<K, V> **entry_ptr = oldtable; i > 0; ++entry_ptr) {
          if(is_used(*entry_ptr)) {
            --i;
            
            unsigned int i2 = lookup((*entry_ptr)->key);
            HASHTABLE_ASSERT(!is_used(table[i2]));
            
            table[i2] = *entry_ptr;
          }
        }
        
        nonnull_count = used_count;
        
        if(oldtable != small_table)
          delete[] oldtable;
      }
      
    public:
      Hashtable()
        : Base()
      {
        SET_BASE_DEBUG_TAG(typeid(*this).name());
        
        nonnull_count = 0;
        used_count    = 0;
        capacity      = MINSIZE;
        table         = small_table;
        memset(table, 0, capacity * sizeof(Entry<K, V> *));
        
        HASHTABLE_ASSERT(is_deleted(Deleted()));
        HASHTABLE_ASSERT(!is_used(Deleted()));
#ifdef RICHMATH_DEBUG_HASHTABLES
        _debug_change_canary = 0;
        _debug_num_iterators = 0;
#endif
      }
      
      ~Hashtable() {
        for(unsigned int i = 0; i < capacity; ++i)
          if(is_used(table[i]))
            delete table[i];
            
        if(table != small_table)
          delete[] table;
      }
      
      unsigned int size() const { return used_count; }
      
      const Entry<K, V> *entry(unsigned int i) const { return is_used(table[i]) ? table[i] : nullptr; }
      Entry<K, V>       *entry(unsigned int i)       { return is_used(table[i]) ? table[i] : nullptr; }
      
      const V *search(const K &key) const {
        unsigned int i = lookup(key);
        if(is_used(table[i]))
          return &table[i]->value;
        return nullptr;
      }
      
      V *search(const K &key) {
        unsigned int i = lookup(key);
        if(is_used(table[i]))
          return &table[i]->value;
        return nullptr;
      }
      
      const Entry<K, V> *search_entry(const K &key) const {
        unsigned int i = lookup(key);
        if(is_used(table[i]))
          return table[i];
        return nullptr;
      }
      
      Entry<K, V> *search_entry(const K &key) {
        unsigned int i = lookup(key);
        if(is_used(table[i]))
          return table[i];
        return nullptr;
      }
      
      V &get(const K &key, V &def) const {
        unsigned int i = lookup(key);
        if(is_used(table[i]))
          return table[i]->value;
        return def;
      }
      
      const V &get(const K &key, const V &def) const {
        unsigned int i = lookup(key);
        if(is_used(table[i]))
          return table[i]->value;
        return def;
      }
      
      V &operator[](const K &key) {
        unsigned int i = lookup(key);
        if(is_used(table[i]))
          return table[i]->value;
        return default_value;
      }
      
      const V &operator[](const K &key) const {
        unsigned int i = lookup(key);
        if(is_used(table[i]))
          return table[i]->value;
        return default_value;
      }
      
      void clear() {
        do_change();
        for(unsigned int i = 0; used_count > 0; ++i) {
          HASHTABLE_ASSERT(i < capacity);
          
          if(is_used(table[i])) {
            --used_count;
            delete table[i];
          }
        }
        
        HASHTABLE_ASSERT(used_count == 0);
        
        if(table != small_table)
          delete[] table;
          
        nonnull_count = 0;
        used_count    = 0;
        capacity      = MINSIZE;
        table         = small_table;
        memset(table, 0, capacity * sizeof(Entry<K, V> *));
      }
      
      void remove(const K &key) {
        unsigned int index = lookup(key);
        do_change();
        if(is_used(table[index])) {
          delete table[index];
          
          --used_count;
          table[index] = Deleted();
        }
      }
      
      void set(const K &key, const V &value) {
        V tmp{value};
        set(key, std::move(tmp));
      }
      
      void set(const K &key, V &&value) {
        unsigned int i = lookup(key);
        do_change();
        if(is_used(table[i])) {
          table[i]->value = std::move(value);
        }
        else {
          if((nonnull_count + 1) * 3 >= capacity * 2) {
            resize(2 * nonnull_count);
            
            set(key, std::move(value));
            return;
          }
          
          if(table[i] == 0)
            ++nonnull_count;
          ++used_count;
          table[i] = new Entry<K, V>(key, std::move(value));
        }
      }
      
      template <typename K2, typename V2, unsigned int (*h2)(const K2 &)>
      void merge(const Hashtable<K2, V2, h2> &other) {
        for(unsigned int i = 0; i < other.capacity; ++i) {
          if(is_used(other.table[i]))
            set(other.table[i]->key, other.table[i]->value);
        }
      }
      
      void swap(Hashtable<K, V, hash_function> &other) {
        using std::swap;
        swap(nonnull_count, other.nonnull_count);
        swap(used_count, other.used_count);
        swap(capacity, other.capacity);
        //bool uses_small_table = table == small_table;
        //bool other_uses_small_table = other.table == other.small_table;
        //if(uses_small_table || other_uses_small_table)
        swap(small_table, other.small_table);
        swap(table, other.table);
        if(other.table == small_table)
          other.table = other.small_table;
        if(table == other.small_table)
          table = small_table;
        swap(default_value, other.default_value);
      }
      
      template <class HT, class E>
      class EntryEnum {
        public:
          EntryEnum(HT &table): _table(table) {
          }
          
          Iterator<E> begin() const {
            E **entries = const_cast<E**>(_table.table);
            if(_table.used_count > 0) {
              while(!is_used(*entries))
                ++entries;
                
              return Iterator<E> {entries, _table.used_count, _table};
            }
            return Iterator<E> {entries, 0, _table};
          }
          
          Iterator<E> end() const {
            E **entries = const_cast<E**>(_table.table);
            return Iterator<E> {entries, 0, _table};
          }
          
        private:
          HT &_table;
      };
      
      EntryEnum<const self_t, const entry_t> entries() const {
        return EntryEnum<const self_t, const entry_t> {*this};
      }
      
      EntryEnum<self_t, entry_t> entries() {
        return EntryEnum<self_t, entry_t> {*this};
      }
  };
  
  template <typename K, typename V, unsigned int (*h)(const K &)>
  inline void swap(Hashtable<K, V, h> &lhs, Hashtable<K, V, h> &rhs) {
    lhs.swap(rhs);
  }
}

#endif // __UTIL__HASHTABLE_H__
