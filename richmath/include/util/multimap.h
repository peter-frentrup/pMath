#ifndef RICHMATH__UTIL__MULTIMAP_H__INCLUDED
#define RICHMATH__UTIL__MULTIMAP_H__INCLUDED

#include <util/hashtable.h>

namespace richmath {
  struct MultiMapEntryGen {
    unsigned generation : 31; // part of the key
    unsigned is_deleted : 1;  // part of the value
  };
  
  template<typename K, typename V>
  struct MultiMapEntry {
    MultiMapEntry(const K &k, MultiMapEntryGen extra, const V &v): key(k), _extra(extra), value(v) {}
    MultiMapEntry(const K &k, MultiMapEntryGen extra, V &&v): key(k), _extra(extra), value(std::move(v)) {}
    MultiMapEntry(const K &k, unsigned generation) : key(k), _extra{generation, 0} {
    }
    
    const MultiMapEntry &operator=(const MultiMapEntry<K, V> &src) = delete;
    
    // only compares key and generation
    friend bool operator==(const MultiMapEntry &left, const MultiMapEntry &right) {
      return left._extra.generation == right._extra.generation && left.key == right.key;
    }
    
    unsigned hash() const { return default_hash(key) + 17 * _extra.generation; }
    
    const K key;
    const MultiMapEntryGen _extra;
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
          
          void remove_and_continue() {
            if(entry) {
              MultiMapEntryGen &extra = const_cast<MultiMapEntryGen&>(entry->key._extra);
              extra.is_deleted = 1;
              operator++();
            }
          }
          
          const ValuesIterator &operator++() {
            if(is_valid()) {
              entry_type k { entry->key.key, entry->key._extra.generation };
              
              do {
                MultiMapEntryGen &k_extra = const_cast<MultiMapEntryGen&>(k._extra);
                k_extra.generation++;
                entry = table->search_entry(k);
              } while(entry && entry->key._extra.is_deleted);
              
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
        
        while(e && e->key._extra.is_deleted) {
          MultiMapEntryGen &extra = const_cast<MultiMapEntryGen&>(k._extra);
          extra.generation++;
          e = table.search_entry(k);
        }
        
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
      bool insert(const K &key, const V &value) { return append(key, std::move(value)); }
      bool insert(const K &key, V &&value) {
        entry_type k { key, 0 };
        Entry<entry_type, Void> *empty = nullptr;
        Entry<entry_type, Void> *e = table.search_entry(k);
        while(e) {
          if(e->key._extra.is_deleted) {
            empty = e;
          }
          else if(e->key.value == value)
            return false;
          
          MultiMapEntryGen &k_extra = const_cast<MultiMapEntryGen&>(k._extra);
          k_extra.generation++;
          e = table.search_entry(k);
        }
        if(empty) {
          entry_type       &new_entry   = const_cast<entry_type&>(      empty->key);
          MultiMapEntryGen &empty_extra = const_cast<MultiMapEntryGen&>(empty->key._extra);
          empty_extra.is_deleted = 0;
          new_entry.value = value;
        }
        else {
          MultiMapEntryGen &k_extra = const_cast<MultiMapEntryGen&>(k._extra);
          k_extra.is_deleted = 0;
          k.value = value;
          table.add(k);
        }
        return true;
      }
      
      void remove_all(const K &key) {
        entry_type k { key, 0 };
        while(table.remove(k)) {
          MultiMapEntryGen &k_extra = const_cast<MultiMapEntryGen&>(k._extra);
          k_extra.generation++;
        }
      }
      
      bool remove(const K &key, const V &value) {
        auto all_for_key = get_all(key);
        auto stop = end(all_for_key);
        for(auto it = begin(all_for_key); it != stop; ++it) {
          if(*it == value) {
            it.remove_and_continue();
            return true;
          }
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
