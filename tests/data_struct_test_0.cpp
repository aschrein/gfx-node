#define UTILS_IMPL
#include "../script.hpp"
#include "../utils.hpp"
#include <stdio.h>

struct Test_Allocator {
  static size_t total_alloced;
  static void * alloc(size_t size) {
    total_alloced += size;
    void *ptr          = tl_alloc(size + sizeof(size_t));
    ((size_t *)ptr)[0] = size;
    return ((u8 *)ptr + 8);
  }
  static void *realloc(void *ptr, size_t old_size, size_t new_size) {
    if (ptr == NULL) {
      ASSERT_ALWAYS(old_size == 0);
      return alloc(new_size);
    }
    total_alloced -= old_size;
    total_alloced += new_size;
    void *old_ptr = (u8 *)ptr - sizeof(size_t);
    ASSERT_ALWAYS(((size_t *)old_ptr)[0] == old_size);
    void *new_ptr = tl_realloc(old_ptr, old_size + sizeof(size_t), new_size + sizeof(size_t));
    ((size_t *)new_ptr)[0] = new_size;
    return ((u8 *)new_ptr + sizeof(size_t));
  }
  static void free(void *ptr) {
    size_t size = ((size_t *)((u8 *)ptr - sizeof(size_t)))[0];
    total_alloced -= size;
    tl_free(((u8 *)ptr - sizeof(size_t)));
  }
};

size_t Test_Allocator::total_alloced = 0;

int main() {
  u32 N = 100;
  ito(N) {
    Array<u32, 0x1000, Test_Allocator> arr;
    arr.init();
    jto(N) { arr.push(j); }
    jto(arr.size) { ASSERT_ALWAYS(arr.ptr[j] == j); }
    jto(N) { ASSERT_ALWAYS(arr.pop() == (N - 1 - j)); }
    ASSERT_ALWAYS(arr.size == 0);
    ASSERT_ALWAYS(Test_Allocator::total_alloced == 0);
    arr.release();
    ASSERT_ALWAYS(Test_Allocator::total_alloced == 0);
  }
  ASSERT_ALWAYS(Test_Allocator::total_alloced == 0);
  char buf[0x100];
  ito(10) {
    Hash_Set<u32, Test_Allocator, 0x100> set;
    set.init();
    Array<u64, 0x100, Test_Allocator> arr;
    arr.init();
    u64 rnd = i;
    jto(N) {
      rnd = hash_of(rnd);
      arr.push(rnd);
      set.insert(rnd);
    }
    jto(N) { ASSERT_ALWAYS(set.contains(arr[j])); }
    arr.release();
    set.release();
    ASSERT_ALWAYS(Test_Allocator::total_alloced == 0);
  }
  ito(10) {
    tl_alloc_tmp_enter();
    Hash_Set<string_ref, Test_Allocator, 0x100> set;
    set.init();
    Array<u64, 0x100, Test_Allocator> arr;
    arr.init();
    u64 rnd = i;
    jto(N) {
      rnd = hash_of(rnd);
      snprintf(buf, sizeof(buf), "key: %lu", rnd);
      string_ref key = stref_tmp(buf);
      arr.push(rnd);
      set.insert(key);
    }
    jto(N) {
      snprintf(buf, sizeof(buf), "key: %lu", arr[j]);
      string_ref key = stref_tmp(buf);
      ASSERT_ALWAYS(set.contains(key));
    }
    jto(N) {
      snprintf(buf, sizeof(buf), "key: %lu", arr[j]);
      string_ref key = stref_tmp(buf);
      ASSERT_ALWAYS(set.remove(key));
    }
    ASSERT_ALWAYS(set.item_count == 0);
    ASSERT_ALWAYS(set.arr.ptr == NULL);
    arr.release();
    set.release();
    tl_alloc_tmp_exit();
    ASSERT_ALWAYS(Test_Allocator::total_alloced == 0);
  }
  ito(10) {
    tl_alloc_tmp_enter();
    Hash_Table<string_ref, u64, Test_Allocator, 0x100> table;
    table.init();
    Array<u64, 0x100, Test_Allocator> arr;
    arr.init();
    u64 rnd = i;
    jto(N) {
      rnd = hash_of(rnd);
      snprintf(buf, sizeof(buf), "key: %lu", rnd);
      string_ref key = stref_tmp(buf);
      arr.push(rnd);
      table.insert(key, rnd);
    }
    jto(N) {
      snprintf(buf, sizeof(buf), "key: %lu", arr[j]);
      string_ref key = stref_tmp(buf);
      ASSERT_ALWAYS(table.contains(key));
      ASSERT_ALWAYS(table.get(key) == arr[j]);
      //      fprintf(stdout, "table[%.*s] = %lu\n", (i32)key.len, key.ptr, table.get(key));
    }
    jto(N) {
      snprintf(buf, sizeof(buf), "key: %lu", arr[j]);
      string_ref key = stref_tmp(buf);
      ASSERT_ALWAYS(table.remove(key));
    }
    ASSERT_ALWAYS(table.set.item_count == 0);
    ASSERT_ALWAYS(table.set.arr.ptr == NULL);
    arr.release();
    table.release();
    tl_alloc_tmp_exit();
    ASSERT_ALWAYS(Test_Allocator::total_alloced == 0);
  }
  {
    tl_alloc_tmp_enter();
    Hash_Set<string_ref, Test_Allocator, 0x10000> set;
    set.init();
    ito(1000) {
      jto(1000) {
        snprintf(buf, sizeof(buf), "key_%i_%i", i, j);
        string_ref key = stref_tmp(buf);
        set.insert(key);
      }
    }
    set.release();
    tl_alloc_tmp_exit();
    ASSERT_ALWAYS(Test_Allocator::total_alloced == 0);
  }
  {
    char const *      source       = R"(
    (main
      (add_node
        (format "my_node %i" 666)
        "Gfx/DrawCall"
        0.0 0.0
        1.0 1.0
      )
    )
    )";
    static Pool<List> list_storage = Pool<List>::create((1 << 10));
    list_storage.enter_scope();
    defer(list_storage.exit_scope());
    struct List_Allocator {
      List *alloc() { return list_storage.alloc_zero(1); }
      void  reset() {}
    } list_allocator;
    List *root = List::parse(stref_s(source), list_allocator);
    NOTNULL(root);
    root->dump();
  }
  ASSERT_ALWAYS(Test_Allocator::total_alloced == 0);
  fprintf(stdout, "[SUCCESS]\n");
  return 0;
}
