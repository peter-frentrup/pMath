#ifndef RICHMATH__UTIL__HASHTABLE_H__INCLUDED
#define RICHMATH__UTIL__HASHTABLE_H__INCLUDED

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include <util/base.h>
#include <pmath-cpp.h> // for PMATH_CPP_MOVE

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
  template<typename T>
  struct default_hash_impl {
    static unsigned int hash(const T &t) {
      return t.hash();
    }
  };
  
  template<typename T>
  unsigned int default_hash(const T &t) {
    return default_hash_impl<T>::hash(t);
  }
  
  struct default_cast_hash_impl {
    static unsigned int for_ui64(uint64_t t) {
      return (unsigned int)((t & 0xFFFFFFFFU) ^ (t >> 32));
    }
  };
  
  template<>
  struct default_hash_impl<unsigned short> {
    static unsigned int hash(unsigned short t) {
      return (unsigned int)t;
    }
  };
  
  template<>
  struct default_hash_impl<unsigned int> {
    static unsigned int hash(unsigned int t) {
      return (unsigned int)t;
    }
  };

  template<>
  struct default_hash_impl<unsigned long> {
    static unsigned int hash(unsigned long t) {
      if(sizeof(t) == sizeof(uint32_t))
        return default_hash((uint32_t)t);
      else
        return default_cast_hash_impl::for_ui64((uint64_t)t);
    }
  };
  
  template<>
  struct default_hash_impl<unsigned long long> {
    static unsigned int hash(unsigned long long t) {
      if(sizeof(t) == sizeof(uint32_t))
        return default_hash((uint32_t)t);
      else
        return default_cast_hash_impl::for_ui64((uint64_t)t);
    }
  };
  
  template<>
  struct default_hash_impl<int> {
    static unsigned int hash(int t) {
      return (unsigned int)t;
    }
  };
  
  template<typename TP>
  struct default_hash_impl<TP *> {
    static unsigned int hash(TP *const &t) {
      return default_hash_impl<uintptr_t>::hash((uintptr_t)t);
    }
  };
  
  class Void { /* Note that this takes up one byte! */
  };
  
  template<typename K, typename V>
  class Entry {
    public:
      Entry(const K &k, const V &v): key(k), value(v) {}
      Entry(const K &k, V &&v): key(k), value(PMATH_CPP_MOVE(v)) {}
      
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
  
  /** An associative dictionary.
      
      This type is trivially relocatable: Its storagy may be moved without needing 
      to call the move constructor.
   */
  template <typename K, typename V>
  class Hashtable {
    private:
      using self_type = Hashtable<K, V>;
      using entry_type = Entry<K, V>;
      
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
      Entry<K, V>   **large_table;
      Entry<K, V>    *small_table[MINSIZE];
#ifdef RICHMATH_DEBUG_HASHTABLES
      unsigned _debug_num_freezers;
#endif
      
      Entry<K, V> *       * table()       { return large_table ? large_table : small_table; }
      Entry<K, V> * const * table() const { return large_table ? large_table : small_table; }

      class TableFreezer {
        public:
          TableFreezer(self_type &_table)
          : table(_table)
          {
#ifdef RICHMATH_DEBUG_HASHTABLES
            table._debug_num_freezers++;
#endif
          }
          
          TableFreezer(const TableFreezer &src)
          : table(src.table)
          {
#ifdef RICHMATH_DEBUG_HASHTABLES
            table._debug_num_freezers++;
#endif
          }
          
          ~TableFreezer() {
#ifdef RICHMATH_DEBUG_HASHTABLES
            HASHTABLE_ASSERT(table._debug_num_freezers > 0);
            table._debug_num_freezers--;
#endif
          }
        
        private:
          TableFreezer const &operator=(TableFreezer const &src) = delete;
          
        public:
          self_type &table;
      };
      
    public:
      V default_value;
      
      template<class E>
      class Iterator {
          friend self_type;
        public:
          using entry_type = E;
        private:
          Iterator(E **entries, unsigned int unused_count, self_type &owning_table)
            : _entries(entries), 
              _unused_count(unused_count),
              _owner(owning_table)
          {
          }
          
        public:
          bool operator!=(const Iterator &other) const {
            return _unused_count != other._unused_count;
          }
          const E &operator*() const {
            HASHTABLE_ASSERT(is_used(*_entries));
            return **_entries;
          }
          E &operator*() {
            HASHTABLE_ASSERT(is_used(*_entries));
            return **_entries;
          }
          const Iterator &operator++() {
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
          TableFreezer _owner;
      };
      
      class MutableIterator {
          friend self_type;
        public:
          using entry_type = typename self_type::entry_type;
        public:
          class DeletableEntry {
              friend class MutableIterator;
            public:
              const K & key;
              V       & value;
              
            public:
              void delete_self() {
                // TODO: ensure that the owning_iter did not move forward
                owning_iter.delete_current(this);
              }
              
            private:
              DeletableEntry(MutableIterator &owner, entry_type &entry)
                : key(entry.key), value(entry.value), owning_iter(owner)
              {
              }
              
            private:
              MutableIterator &owning_iter;
          };
          
        private:
          MutableIterator(entry_type **entries, unsigned int unused_count, self_type &owning_table)
            : _entries(entries), 
              _unused_count(unused_count),
              _owner(owning_table)
          {
          }
          
        public:
          bool operator!=(const MutableIterator &other) const {
            return _unused_count != other._unused_count;
          }
          const entry_type operator*() const {
            HASHTABLE_ASSERT(is_used(*_entries));
            return **_entries;
          }
          DeletableEntry operator*() {
            HASHTABLE_ASSERT(is_used(*_entries));
            return DeletableEntry{ *this, **_entries };
          }
          const MutableIterator &operator++() {
            while(_unused_count > 0) {
              ++_entries;
              if(is_used(*_entries)) {
                --_unused_count;
                break;
              }
            }
            return *this;
          }
          
          void delete_current(DeletableEntry *requester = nullptr) {
            HASHTABLE_ASSERT(is_used(*_entries));
            if(requester)
              HASHTABLE_ASSERT(&requester->key == &(**_entries).key);
            
#ifdef RICHMATH_DEBUG_HASHTABLES
            _owner.table._debug_num_freezers-= 2; // this, end()
#endif
            
            // TODO: directly set * _entries instead of a new lookup
            _owner.table.remove((**_entries).key);
            
#ifdef RICHMATH_DEBUG_HASHTABLES
            _owner.table._debug_num_freezers+= 2; // this, end()
#endif
            
            HASHTABLE_ASSERT(!is_used(*_entries));
          }
          
        private:
          entry_type **_entries;
          unsigned int _unused_count;
          TableFreezer _owner;
      };
      
      class KeyIterator {
          friend self_type;
        private:
          Iterator<const Entry<K, V>> _entry_iter;
          
          KeyIterator(Iterator<const Entry<K, V>> entry_iter)
            : _entry_iter(entry_iter)
          {
          }
          
        public:
          bool operator!=(const KeyIterator &other) const {
            return _entry_iter != other._entry_iter;
          }
          const K &operator*() const {
            return (*_entry_iter).key;
          }
          const KeyIterator &operator++() {
            ++_entry_iter;
            return *this;
          }
      };
      
      using iterator_type       = Iterator<Entry<K, V>>;
      using const_iterator_type = Iterator<const Entry<K, V>>;
      using key_iterator_type   = KeyIterator;
      
    private:
      void do_change() { 
        HASHTABLE_ASSERT((_debug_num_freezers == 0) && "cannot mutate hashtable while there are still iterators to it/the table is freezed");
      }

      unsigned int lookup(const K &key) const {
        unsigned int freeslot = -(unsigned int)1;
        unsigned int h        = default_hash(key);
        unsigned int index    = h & (capacity - 1);
        auto tab = table();
        
        for(;;) {
          if(!tab[index]) {
            if(freeslot == -(unsigned int)1)
              return index;
            return freeslot;
          }
          
          if(is_deleted(tab[index])) {
            if(freeslot == -(unsigned int)1)
              freeslot = index;
          }
          else if(tab[index]->key == key)
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
        Entry<K, V> **oldtable = table();
        if(capacity == MINSIZE)
          large_table = nullptr;
        else
          large_table = new Entry<K, V> *[capacity];
          
        Entry<K, V> **newtable = table();
          
        HASHTABLE_ASSERT(newtable != oldtable);
        
        memset(newtable, 0, newsize * sizeof(Entry<K, V> *));
        
        unsigned int i = used_count;
        for(Entry<K, V> **entry_ptr = oldtable; i > 0; ++entry_ptr) {
          if(is_used(*entry_ptr)) {
            --i;
            
            unsigned int i2 = lookup((*entry_ptr)->key);
            HASHTABLE_ASSERT(!is_used(newtable[i2]));
            
            newtable[i2] = *entry_ptr;
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
        large_table   = nullptr;
        memset(small_table, 0, capacity * sizeof(Entry<K, V> *));
        
        HASHTABLE_ASSERT(is_deleted(Deleted()));
        HASHTABLE_ASSERT(!is_used(Deleted()));
#ifdef RICHMATH_DEBUG_HASHTABLES
        _debug_num_freezers = 0;
#endif
      }
      
      Hashtable(const Hashtable &src) = delete;
      Hashtable const &operator=(Hashtable const &src) = delete;
      
      ~Hashtable() {
        auto tab = table();
        for(unsigned int i = 0; i < capacity; ++i)
          if(is_used(tab[i])) {
            auto old_entry = tab[i];
            tab[i] = Deleted();
            delete old_entry;
          }
            
        if(large_table)
          delete[] large_table;
      }
      
      Hashtable(self_type &&other)
        : Hashtable()
      {
        this->swap(other);
      }
      
      self_type &operator=(self_type &&other) {
        this->swap(other);
        return *this;
      }
      
      friend void swap(Hashtable &lhs, Hashtable &rhs) {
        lhs.swap(rhs);
      }
      
      unsigned int size() const { return used_count; }
      
      const Entry<K, V> *entry(unsigned int i) const { auto tab = table(); return is_used(tab[i]) ? tab[i] : nullptr; }
      Entry<K, V>       *entry(unsigned int i)       { auto tab = table(); return is_used(tab[i]) ? tab[i] : nullptr; }
      
      const V *search(const K &key) const {
        auto tab = table();
        unsigned int i = lookup(key);
        if(is_used(tab[i]))
          return &tab[i]->value;
        return nullptr;
      }
      
      V *search(const K &key) {
        auto tab = table();
        unsigned int i = lookup(key);
        if(is_used(tab[i]))
          return &tab[i]->value;
        return nullptr;
      }
      
      const Entry<K, V> *search_entry(const K &key) const {
        auto tab = table();
        unsigned int i = lookup(key);
        if(is_used(tab[i]))
          return tab[i];
        return nullptr;
      }
      
      Entry<K, V> *search_entry(const K &key) {
        auto tab = table();
        unsigned int i = lookup(key);
        if(is_used(tab[i]))
          return tab[i];
        return nullptr;
      }
      
      V &get(const K &key, V &def) const {
        auto tab = table();
        unsigned int i = lookup(key);
        if(is_used(tab[i]))
          return tab[i]->value;
        return def;
      }
      
      const V &get(const K &key, const V &def) const {
        auto tab = table();
        unsigned int i = lookup(key);
        if(is_used(tab[i]))
          return tab[i]->value;
        return def;
      }
      
      V &operator[](const K &key) {
        auto tab = table();
        unsigned int i = lookup(key);
        if(is_used(tab[i]))
          return tab[i]->value;
        return default_value;
      }
      
      const V &operator[](const K &key) const {
        auto tab = table();
        unsigned int i = lookup(key);
        if(is_used(tab[i]))
          return tab[i]->value;
        return default_value;
      }
      
      void clear() {
        auto tab = table();
        do_change();
        for(unsigned int i = 0; used_count > 0; ++i) {
          HASHTABLE_ASSERT(i < capacity);
          
          if(is_used(tab[i])) {
            auto old_entry = tab[i];
            tab[i] = Deleted();
            --used_count;
            delete old_entry;
          }
        }
        
        HASHTABLE_ASSERT(used_count == 0);
        
        if(large_table)
          delete[] large_table;
          
        nonnull_count = 0;
        used_count    = 0;
        capacity      = MINSIZE;
        large_table   = nullptr;
        memset(small_table, 0, capacity * sizeof(Entry<K, V> *));
      }
      
      // return whether the key existed
      bool remove(const K &key) {
        auto tab = table();
        unsigned int index = lookup(key);
        do_change();
        if(!is_used(tab[index])) 
          return false;
        
        auto old_entry = tab[index];
        tab[index] = Deleted();
        --used_count;
        delete old_entry;
        return true;
      }
      
      // return whether the value was modified
      template<typename F>
      bool modify(const K &key, const V &value, F values_are_equal) {
        V tmp{value};
        return modify(key, PMATH_CPP_MOVE(tmp), values_are_equal);
      }
      
      // return whether the value was modified
      template<typename F>
      bool modify(const K &key, V &&value, F values_are_equal) {
        auto tab = table();
        unsigned int i = lookup(key);
        do_change();
        if(is_used(tab[i])) {
          if(values_are_equal(tab[i]->value, value))
            return false;
          
          tab[i]->value = PMATH_CPP_MOVE(value);
          return true;
        }
        
        if((nonnull_count + 1) * 3 >= capacity * 2) {
          resize(2 * nonnull_count);
          
          return modify(key, PMATH_CPP_MOVE(value), values_are_equal);
        }
        
        if(tab[i] == 0)
          ++nonnull_count;
        ++used_count;
        tab[i] = new Entry<K, V>(key, PMATH_CPP_MOVE(value));
        return true;
      }
      
      // return whether the value was added
      bool set(const K &key, const V &value) {
        V tmp{value};
        return set(key, PMATH_CPP_MOVE(tmp));
      }
      
      // return whether the value added
      bool set(const K &key, V &&value) {
        return modify(key, PMATH_CPP_MOVE(value), [](const V &left, const V &right) { return false; });
      }
      
      // return whether the value was added
      bool set_default(const K &key, const V &value) {
        V tmp{value};
        return set_default(key, PMATH_CPP_MOVE(tmp));
      }
      
      // return whether the value added
      bool set_default(const K &key, V &&value) {
        return modify(key, PMATH_CPP_MOVE(value), [](const V &left, const V &right) { return true; });
      }
      
      template <typename K2, typename V2>
      void merge(const Hashtable<K2, V2> &other) {
        auto other_tab = other.table();
        for(unsigned int i = 0; i < other.capacity; ++i) {
          if(is_used(other_tab[i]))
            set(other_tab[i]->key, other_tab[i]->value);
        }
      }
      
      template <typename K2, typename V2>
      void merge_defaults(const Hashtable<K2, V2> &defaults) {
        auto def_tab = defaults.table();
        for(unsigned int i = 0; i < defaults.capacity; ++i) {
          if(is_used(def_tab[i]))
            set_default(def_tab[i]->key, def_tab[i]->value);
        }
      }
      
      void swap(self_type &other) {
        using std::swap;
        swap(nonnull_count, other.nonnull_count);
        swap(used_count, other.used_count);
        swap(capacity, other.capacity);
        swap(small_table, other.small_table);
        swap(large_table, other.large_table);
        swap(default_value, other.default_value);
      }
      
      template <class HT, class It>
      class EntryEnum {
          using entry_type         = typename It::entry_type;
          using mutable_table_type = typename HT::self_type;
        public:
          EntryEnum(HT &table): _table(table) {
          }
          
          It begin() const {
            entry_type **entries = const_cast<entry_type **>(_table.table());
            if(_table.used_count > 0) {
              while(!is_used(*entries))
                ++entries;
                
              return It {entries, _table.used_count, const_cast<mutable_table_type&>(_table)};
            }
            return It {entries, 0, const_cast<mutable_table_type&>(_table)};
          }
          
          It end() const {
            entry_type **entries = const_cast<entry_type **>(_table.table());
            return It {entries, 0, const_cast<mutable_table_type&>(_table)};
          }
          
        private:
          HT &_table;
      };
      
      class KeyEnum {
        public:
          KeyEnum(EntryEnum<const self_type, const_iterator_type> entries): _entries(entries) {
          }
        
          KeyIterator begin() const {
            return KeyIterator(_entries.begin());
          }
          
          KeyIterator end() const {
            return KeyIterator(_entries.end());
          }
          
        private:
          EntryEnum<const self_type, const_iterator_type>  _entries;
      };
      
      EntryEnum<const self_type, const_iterator_type> entries() const {
        return EntryEnum<const self_type, const_iterator_type> {*this};
      }
      
      EntryEnum<self_type, iterator_type> entries() {
        return EntryEnum<self_type, iterator_type> {*this};
      }
      
      KeyEnum keys() const {
        return KeyEnum {entries()};
      }
      
      EntryEnum<self_type, MutableIterator> deletable_entries() {
        return EntryEnum<self_type, MutableIterator> {*this};
      }
  };
  
  template<typename K>
  class Hashset : public Hashtable<K, Void> {
      using KeyIterator = typename Hashtable<K, Void>::KeyIterator;
    public:
      bool add(const K &key) { return this->set(key, Void{}); }
      bool contains(const K &key) { return this->search(key) != nullptr; }
      
      KeyIterator begin() const {
        return this->keys().begin();
      }
      
      KeyIterator end() const {
        return this->keys().end();
      }
  };
}

#endif // RICHMATH__UTIL__HASHTABLE_H__INCLUDED
