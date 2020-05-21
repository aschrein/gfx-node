#include "utils.hpp"

struct List {
  string_ref symbol = {};
  u64        id     = 0;
  List *     child  = NULL;
  List *     next   = NULL;
  string_ref get_symbol() {
    ASSERT_ALWAYS(nonempty());
    return symbol;
  }
  bool nonempty() { return symbol.ptr != 0 && symbol.len != 0; }
  bool cmp_symbol(char const *str) {
    if (symbol.ptr == NULL) return false;
    return symbol == stref_s(str);
  }
  bool has_child(char const *name) { return child != NULL && child->cmp_symbol(name); }
  template <typename T> void match_children(char const *name, T on_match) {
    if (child != NULL) {
      if (child->cmp_symbol(name)) {
        on_match(child);
      }
      child->match_children(name, on_match);
    }
    if (next != NULL) {
      next->match_children(name, on_match);
    }
  }
  List *get(u32 i) {
    List *cur = this;
    while (i != 0) {
      if (cur == NULL) return NULL;
      cur = cur->next;
      i -= 1;
    }
    return cur;
  }

  int ATTR_USED dump(u32 indent = 0) const {
    ito(indent) fprintf(stdout, " ");
    if (symbol.ptr != NULL) {
      fprintf(stdout, "%.*s\n", (i32)symbol.len, symbol.ptr);
    } else {
      fprintf(stdout, "$\n");
    }
    if (child != NULL) {
      child->dump(indent + 2);
    }
    if (next != NULL) {
      next->dump(indent);
    }
    fflush(stdout);
    return 0;
  }
  void dump_list_graph() {
    List *root     = this;
    FILE *dotgraph = fopen("list.dot", "wb");
    fprintf(dotgraph, "digraph {\n");
    fprintf(dotgraph, "node [shape=record];\n");
    tl_alloc_tmp_enter();
    defer(tl_alloc_tmp_exit());
    List **stack        = (List **)tl_alloc_tmp(sizeof(List *) * (1 << 10));
    u32    stack_cursor = 0;
    List * cur          = root;
    u64    null_id      = 0xffffffffull;
    while (cur != NULL || stack_cursor != 0) {
      if (cur == NULL) {
        cur = stack[--stack_cursor];
      }
      ASSERT_ALWAYS(cur != NULL);
      if (cur->symbol.ptr != NULL) {
        ASSERT_ALWAYS(cur->symbol.len != 0);
        fprintf(dotgraph, "%lu [label = \"%.*s\", shape = record];\n", cur->id,
                (int)cur->symbol.len, cur->symbol.ptr);
      } else {
        fprintf(dotgraph, "%lu [label = \"$\", shape = record, color=red];\n", cur->id);
      }
      if (cur->next == NULL) {
        fprintf(dotgraph, "%lu [label = \"nil\", shape = record, color=blue];\n", null_id);
        fprintf(dotgraph, "%lu -> %lu [label = \"next\"];\n", cur->id, null_id);
        null_id++;
      } else
        fprintf(dotgraph, "%lu -> %lu [label = \"next\"];\n", cur->id, cur->next->id);

      if (cur->child != NULL) {
        if (cur->next != NULL) stack[stack_cursor++] = cur->next;
        fprintf(dotgraph, "%lu -> %lu [label = \"child\"];\n", cur->id, cur->child->id);
        cur = cur->child;
      } else {
        cur = cur->next;
      }
    }
    fprintf(dotgraph, "}\n");
    fflush(dotgraph);
    fclose(dotgraph);
  }
  template <typename T> static List *parse(string_ref text, T allocator) {
    List *root = allocator.alloc();
    List *cur  = root;
    TMP_STORAGE_SCOPE;
    List **stack        = (List **)tl_alloc_tmp(sizeof(List *) * (1 << 8));
    u32    stack_cursor = 0;
    enum class State : char {
      UNDEFINED = 0,
      SAW_QUOTE,
      SAW_LPAREN,
      SAW_RPAREN,
      SAW_PRINTABLE,
      SAW_SEPARATOR,
    };
    u32   i  = 0;
    u64   id = 1;
    State state_table[0x100];
    memset(state_table, 0, sizeof(state_table));
    for (u8 j = 0x20; j <= 0x7f; j++) state_table[j] = State::SAW_PRINTABLE;
    state_table[(u32)'(']  = State::SAW_LPAREN;
    state_table[(u32)')']  = State::SAW_RPAREN;
    state_table[(u32)'"']  = State::SAW_QUOTE;
    state_table[(u32)' ']  = State::SAW_SEPARATOR;
    state_table[(u32)'\n'] = State::SAW_SEPARATOR;
    state_table[(u32)'\t'] = State::SAW_SEPARATOR;
    state_table[(u32)'\r'] = State::SAW_SEPARATOR;

    auto next_item = [&]() {
      List *next = allocator.alloc();
      next->id   = id++;
      if (cur != NULL) cur->next = next;
      cur = next;
    };

    auto push_item = [&]() {
      List *new_head = allocator.alloc();
      new_head->id   = id++;
      if (cur != NULL) {
        stack[stack_cursor++] = cur;
        cur->child            = new_head;
      }
      cur = new_head;
    };

    auto pop_item = [&]() -> bool {
      if (stack_cursor == 0) {
        return false;
      }
      cur = stack[--stack_cursor];
      return true;
    };

    auto append_char = [&]() {
      if (cur->symbol.ptr == NULL) { // first character for that item
        cur->symbol.ptr = text.ptr + i;
      }
      cur->symbol.len++;
    };

    auto cur_non_empty = [&]() { return cur != NULL && cur->symbol.len != 0; };
    auto cur_has_child = [&]() { return cur != NULL && cur->child != NULL; };

    i                = 0;
    State prev_state = State::UNDEFINED;
    while (i < text.len) {
      char  c     = text.ptr[i];
      State state = state_table[(u8)c];
      switch (state) {
      case State::UNDEFINED: {
        goto error_parsing;
      }
      case State::SAW_QUOTE: {
        if (cur_non_empty() || cur_has_child()) next_item();
        if (text.ptr[i + 1] == '"' && text.ptr[i + 2] == '"') {
          i += 3;
          while (
            text.ptr[i + 0] != '"' || //
            text.ptr[i + 1] != '"' || //
            text.ptr[i + 2] != '"'
          ) {
            append_char();
            i += 1;
          }
          i += 2;
        } else {
          i += 1;
          while (text.ptr[i] != '"') {
            append_char();
            i += 1;
          }
        }
        next_item();
        break;
      }
      case State::SAW_LPAREN: {
        if (cur_has_child() || cur_non_empty()) next_item();
        push_item();
        break;
      }
      case State::SAW_RPAREN: {
        if (pop_item() == false) goto exit_loop;
        break;
      }
      case State::SAW_SEPARATOR: {
        //      if (cur_non_empty()) next_item();
        break;
      }
      case State::SAW_PRINTABLE: {
        if (cur_has_child()) next_item();
        if (cur_non_empty() && prev_state != State::SAW_PRINTABLE) next_item();
        append_char();
        break;
      }
      }
      prev_state = state;
      i += 1;
    }
  exit_loop:
    (void)0;
    return root;
  error_parsing:
    allocator.reset();
    return NULL;
  }
};

static inline bool parse_decimal_int(char const *str, size_t len, int32_t *result) {
  int32_t  final = 0;
  int32_t  pow   = 1;
  int32_t  sign  = 1;
  uint32_t i     = 0;
  // parsing in reverse order
  for (; i < len; ++i) {
    switch (str[len - 1 - i]) {
    case '0': break;
    case '1': final += 1 * pow; break;
    case '2': final += 2 * pow; break;
    case '3': final += 3 * pow; break;
    case '4': final += 4 * pow; break;
    case '5': final += 5 * pow; break;
    case '6': final += 6 * pow; break;
    case '7': final += 7 * pow; break;
    case '8': final += 8 * pow; break;
    case '9': final += 9 * pow; break;
    // it's ok to have '-'/'+' as the first char in a string
    case '-': {
      if (i == len - 1)
        sign = -1;
      else
        return false;
      break;
    }
    case '+': {
      if (i == len - 1)
        sign = 1;
      else
        return false;
      break;
    }
    default: return false;
    }
    pow *= 10;
  }
  *result = sign * final;
  return true;
}

static inline bool parse_float(char const *str, size_t len, float *result) {
  float    final = 0.0f;
  uint32_t i     = 0;
  float    sign  = 1.0f;
  if (str[0] == '-') {
    sign = -1.0f;
    i    = 1;
  }
  for (; i < len; ++i) {
    if (str[i] == '.') break;
    switch (str[i]) {
    case '0': final = final * 10.0f; break;
    case '1': final = final * 10.0f + 1.0f; break;
    case '2': final = final * 10.0f + 2.0f; break;
    case '3': final = final * 10.0f + 3.0f; break;
    case '4': final = final * 10.0f + 4.0f; break;
    case '5': final = final * 10.0f + 5.0f; break;
    case '6': final = final * 10.0f + 6.0f; break;
    case '7': final = final * 10.0f + 7.0f; break;
    case '8': final = final * 10.0f + 8.0f; break;
    case '9': final = final * 10.0f + 9.0f; break;
    default: return false;
    }
  }
  i++;
  float pow = 1.0e-1f;
  for (; i < len; ++i) {
    switch (str[i]) {
    case '0': break;
    case '1': final += 1.0f * pow; break;
    case '2': final += 2.0f * pow; break;
    case '3': final += 3.0f * pow; break;
    case '4': final += 4.0f * pow; break;
    case '5': final += 5.0f * pow; break;
    case '6': final += 6.0f * pow; break;
    case '7': final += 7.0f * pow; break;
    case '8': final += 8.0f * pow; break;
    case '9': final += 9.0f * pow; break;
    default: return false;
    }
    pow *= 1.0e-1f;
  }
  *result = sign * final;
  return true;
}
