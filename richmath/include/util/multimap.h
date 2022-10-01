#ifndef RICHMATH__UTIL__MULTIMAP_H__INCLUDED
#define RICHMATH__UTIL__MULTIMAP_H__INCLUDED

#include <util/hashtable.h>

namespace richmath {
  template<typename K, typename V>
  struct MultiMapEntry {
    MultiMapEntry(const K &k, int generation, const V &v): key(k), _generation(generation), value(v) {}
    MultiMapEntry(const K &k, int generation, V &&v): key(k), _generation(generation), value(std::move(v)) {}
    MultiMapEntry(const K &k, int generation) : key(k), _generation{generation} {
    }
    
    const MultiMapEntry &operator=(const MultiMapEntry<K, V> &src) = delete;
    
    // only compares key and generation
    friend bool operator==(const MultiMapEntry &left, const MultiMapEntry &right) {
      return left._generation == right._generation && left.key == right.key;
    }
    
    unsigned hash() const { return default_hash(key) + 17 * _generation; }
    
    const K key;
    const int _generation;
    V value;
  };
  
  /** Map a key to zero or more distinct values.
   */
  template<typename K, typename V>
  class MultiMap {
    public:
      using self_type = MultiMap<K, V>;
      using entry_type = MultiMapEntry<K, V>;
      
      class ValuesIterator {
          friend self_type;
        public:
          ValuesIterator() : table{nullptr}, entry{nullptr} {}
          explicit operator bool() const { return is_valid(); }
          
          bool operator!=(const ValuesIterator &other) { return !operator==(other); }
          bool operator==(const ValuesIterator &other) { return table == other.table && entry == entry; }
          
          V       &operator*() {       HASHTABLE_ASSERT(is_valid()); return const_cast<V&>(entry->key.value); }
          const V &operator*() const { HASHTABLE_ASSERT(is_valid()); return entry->key.value; }
          
          const ValuesIterator &operator++() {
            if(is_valid()) {
              entry_type k { entry->key.key, entry->key._generation + 1 };
              entry = table->search_entry(k);
              
              if(!entry)
                table = nullptr;
            }
            return *this;
          }
          
        private:
          bool is_valid() const { return table && entry; }
          
        private:
          Hashset<entry_type>     *table;
          Entry<entry_type, Void> *entry;
      };
      
      class ValuesIterable {
          friend self_type;
        public:
          explicit operator bool() const { return !is_empty(); }
          bool is_empty() const { return !first; }
          
          friend ValuesIterator begin(const ValuesIterable &self) { return self.first; }
          friend ValuesIterator end(const ValuesIterable &self) { return {}; }
        private:
          ValuesIterator first;
      };
      
    public:
      ValuesIterable operator[](const K &key) { return get_all(key); }
      
      ValuesIterable get_all(const K &key) {
        entry_type k { key, 0 };
        Entry<entry_type, Void> *e = table.search_entry(k);
        
        ValuesIterable res;
        if(e) {
          res.first.table = &table;
          res.first.entry = e;
        }
        else {
          res.first.table = nullptr;
          res.first.entry = nullptr;
        }
        return res;
      }
      
      /** Insert a (key,value). Running time should be of order O(#keys + #values_per_key)
       */
      bool insert(const K &key, const V &value) {
        entry_type k { key, 0 };
        Entry<entry_type, Void> *e = table.search_entry(k);
        while(e) {
          if(e->key.value == value)
            return false;
          
          int &k_gen = const_cast<int&>(k._generation);
          k_gen++;
          e = table.search_entry(k);
        }
        k.value = value;
        table.add(k);
        return true;
      }
      
      void remove_all(const K &key) {
        entry_type k { key, 0 };
        while(table.remove(k)) {
          int &k_gen = const_cast<int&>(k._generation);
          k_gen++;
        }
      }
      
      bool remove(const K &key, const V &value) {
        entry_type k { key, 0 };
        int &k_gen = const_cast<int&>(k._generation);
        
        Entry<entry_type, Void> *e = table.search_entry(k);
        
        while(e) {
          k_gen++;
          if(e->key.value == value) {
            auto next = table.search_entry(k);
            while(next) {
              using std::swap;
              V &e_value    = const_cast<V &>(e->key.value);
              V &next_value = const_cast<V &>(next->key.value);
              swap(e_value, next_value);
              
              e = next;
              k_gen++;
              next = table.search_entry(k);
            }
            return table.remove(e->key);
          }
          e = table.search_entry(k);
        }
        
        return false;
      }
      
      void clear() { table.clear(); }
      
    private:
      Hashset<entry_type> table;
  };
  
#ifdef NDEBUG
#  define debug_test_multimap()  ((void)0)
#else
  void debug_test_multimap();
#endif
}

#endif // RICHMATH__UTIL__MULTIMAP_H__INCLUDED
