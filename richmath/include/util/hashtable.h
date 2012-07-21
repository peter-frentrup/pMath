#ifndef __UTIL__HASHTABLE_H__
#define __UTIL__HASHTABLE_H__

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <util/base.h>


namespace richmath {
  template<class T>
  unsigned int object_hash(const T &t) {
    return t.hash();
  }
  
  template<typename T>
  unsigned int cast_hash(const T &t) {
    return (unsigned int)t;
  }
  
  class Void { /* Note that this takes up one byte! */
  };
  
  template<typename K, typename V>
  class Entry {
    public:
      Entry(const K &k, const V &v): key(k), value(v) {}
      
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
      static const unsigned int MINSIZE = 8; // power of 2, >= 2
      static Entry<K, V> *Deleted() { return (Entry<K, V> *)(-(size_t)1); }
      
      static bool is_used(Entry<K, V> *e) {
        return ((size_t)e) + 1 > 1;
      }
      
      static bool is_deleted(Entry<K, V> *e) {
        return ((size_t)e) + 1 == 0;
      }
      
    private:
      unsigned int   nonnull_count; // used_count <= nonnull_count < capacity
      unsigned int   used_count;
      unsigned int   capacity;      // power of 2, >= MINSIZE
      Entry<K, V>   **table;
      Entry<K, V>    *small_table[MINSIZE];
      
    public:
      V default_value;
      
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
          
        assert(table != oldtable);
        
        memset(table, 0, newsize * sizeof(Entry<K, V> *));
        
        unsigned int i = used_count;
        for(Entry<K, V> **entry_ptr = oldtable; i > 0; ++entry_ptr) {
          if(is_used(*entry_ptr)) {
            --i;
            
            int i2 = lookup((*entry_ptr)->key);
            assert(!is_used(table[i2]));
            
            table[i2] = *entry_ptr;
          }
        }
        
        nonnull_count = used_count;
        
        if(oldtable != small_table)
          delete[] oldtable;
      }
      
    public:
      Hashtable() {
        nonnull_count = 0;
        used_count    = 0;
        capacity      = MINSIZE;
        table         = small_table;
        memset(table, 0, capacity * sizeof(Entry<K, V> *));
        
        assert(is_deleted(Deleted()));
        assert(!is_used(Deleted()));
      }
      
      ~Hashtable() {
        for(unsigned int i = 0; i < capacity; ++i)
          if(is_used(table[i]))
            delete table[i];
            
        if(table != small_table)
          delete[] table;
      }
      
      unsigned int size() const { return used_count; }
      
      const Entry<K, V> *entry(unsigned int i) const { return is_used(table[i]) ? table[i] : 0; }
      Entry<K, V>       *entry(unsigned int i)       { return is_used(table[i]) ? table[i] : 0; }
      
      V *search(const K &key) const {
        unsigned int i = lookup(key);
        if(is_used(table[i]))
          return &table[i]->value;
        return 0;
      }
      
      Entry<K, V> *search_entry(const K &key) const {
        unsigned int i = lookup(key);
        if(is_used(table[i]))
          return table[i];
        return 0;
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
        for(unsigned int i = 0; used_count > 0; ++i) {
          assert(i < capacity);
          
          if(is_used(table[i])) {
            --used_count;
            delete table[i];
          }
        }
        
        assert(used_count == 0);
        
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
        
        if(is_used(table[index])) {
          delete table[index];
          
          --used_count;
          table[index] = Deleted();
        }
      }
      
      void set(const K &key, const V &value) {
        int i = lookup(key);
        if(is_used(table[i])) {
          table[i]->value = value;
        }
        else {
          if((nonnull_count + 1) * 3 >= capacity * 2) {
            resize(2 * nonnull_count);
            
            set(key, value);
            return;
          }
          
          if(table[i] == 0)
            ++nonnull_count;
          ++used_count;
          table[i] = new Entry<K, V>(key, value);
        }
      }
      
      template <typename K2, typename V2, unsigned int (*h2)(const K2 &)>
      void merge(Hashtable<K2, V2, h2> &other) {
        for(unsigned int i = 0; i < other.capacity; ++i) {
          if(is_used(other.table[i]))
            set(other.table[i]->key, other.table[i]->value);
        }
      }
  };
}

#endif // __UTIL__HASHTABLE_H__
