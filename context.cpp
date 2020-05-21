#include "node_editor.h"
#include "nodes.hpp"
#include "script.hpp"
#include "simplefont.h"

//static inline u16 f32_to_u16(f32 x) { return (u16)(clamp(x, 0.0f, 1.0f) * ((1 << 16) - 1)); }

static inline float3 parse_color_float3(char const *str) {
  ASSERT_ALWAYS(str[0] == '#');
  auto hex_to_decimal = [](char c) {
    if (c >= '0' && c <= '9') {
      return (u32)c - (u32)'0';
    } else if (c >= 'a' && c <= 'f') {
      return 10 + (u32)c - (u32)'a';
    } else if (c >= 'A' && c <= 'F') {
      return 10 + (u32)c - (u32)'A';
    }
    UNIMPLEMENTED;
  };
  u32 r = hex_to_decimal(str[1]) * 16 + hex_to_decimal(str[2]);
  u32 g = hex_to_decimal(str[3]) * 16 + hex_to_decimal(str[4]);
  u32 b = hex_to_decimal(str[5]) * 16 + hex_to_decimal(str[6]);
  return (float3){(f32)r / 255.0f, (f32)g / 255.0f, (f32)b / 255.0f};
}

u32 parse_color_u32(char const *str) {
  ASSERT_ALWAYS(str[0] == '#');
  auto hex_to_decimal = [](char c) {
    if (c >= '0' && c <= '9') {
      return (u32)c - (u32)'0';
    } else if (c >= 'a' && c <= 'f') {
      return 10 + (u32)c - (u32)'a';
    } else if (c >= 'A' && c <= 'F') {
      return 10 + (u32)c - (u32)'A';
    }
    UNIMPLEMENTED;
  };
  u32 r = hex_to_decimal(str[1]) * 16 + hex_to_decimal(str[2]);
  u32 g = hex_to_decimal(str[3]) * 16 + hex_to_decimal(str[4]);
  u32 b = hex_to_decimal(str[5]) * 16 + hex_to_decimal(str[6]);
  return r | (g << 8) | (b << 16);
}

struct _String2D {
  char *   c_str;
  uint32_t len;
  float    x, y, z;
  Color    color;
  bool     world_space;
};
// static Temporary_Storage<>          ts             = Temporary_Storage<>::create(16 * (1 << 20));
static Temporary_Storage<Line2D>    line_storage   = Temporary_Storage<Line2D>::create(1 << 17);
static Temporary_Storage<Rect2D>    quad_storage   = Temporary_Storage<Rect2D>::create(1 << 17);
static Temporary_Storage<_String2D> string_storage = Temporary_Storage<_String2D>::create(1 << 18);
static Temporary_Storage<char>      char_storage   = Temporary_Storage<char>::create(1 * (1 << 20));

struct Source {
  // Name and text are also zero terminated
  string_ref name;
  string_ref text;
  u8 *       storage;
  bool       is_alive() { return storage != NULL; }
  void       init(string_ref name, string_ref text) {
    ASSERT_DEBUG(name.len != 0);
    storage = (u8 *)malloc(name.len + text.len + 2);
    memcpy(storage, name.ptr, name.len);
    storage[name.len] = '\0';
    if (text.ptr != NULL && text.len != 0) {
      memcpy(storage + name.len + 1, text.ptr, text.len);
    }
    storage[name.len + text.len + 1] = '\0';
    this->name = string_ref{.ptr = (char const *)storage, .len = name.len};
    this->text = string_ref{.ptr = (char const *)(storage + name.len + 1), .len = text.len};
  }
  void release() {
    free(storage);
    memset(this, 0, sizeof(*this));
  }
};

struct SourceDB {
  Array<Source>               sources;
  Array<char const *>         names_packed;
  Hash_Table<string_ref, u32> name2id;
  void                        init() {
    sources.init();
    name2id.init();
  }
  void release() {
    sources.release();
    name2id.release();
  }
  void rebuild_index() {
    names_packed.release();
    ito(sources.size) {
      if (sources[i].storage != NULL) {
        names_packed.push(sources[i].name.ptr);
      }
    }
  }
  void remove_source(string_ref name) {
    ASSERT_DEBUG(name2id.contains(name));
    u32 id = name2id.get(name);
    name2id.remove(name);
    sources[id].release();
  }
  void add_source(string_ref name, string_ref text) {
    if (name2id.contains(name)) {
      remove_source(name);
    }
    ASSERT_DEBUG(!name2id.contains(name));
    Source src;
    src.init(name, text);
    // linear search for a new slot
    u32 new_slot = 0;
    ito(sources.size) {
      if (sources[i].storage == NULL) {
        break;
      }
      new_slot++;
    }
    if (new_slot == sources.size)
      sources.push(src);
    else
      sources[new_slot] = src;
    name2id.insert(src.name, new_slot);
  }
  void update_text(string_ref name, string_ref new_text) {
    tl_alloc_tmp_enter();
    defer(tl_alloc_tmp_exit());
    name = stref_tmp_copy(name);
    remove_source(name);
    add_source(name, new_text);
  }
  string_ref get_text(string_ref name) {
    ASSERT_DEBUG(name2id.contains(name));
    u32 id = name2id.get(name);
    return sources[id].text;
  }
};

struct NodeDB {
  struct Node_Wrapper {
    u32                node_id;
    string_ref         node_name;
    SmallArray<u32, 8> out_links;
    SmallArray<u32, 8> in_links;
    void               init() {
      out_links.init();
      in_links.init();
    }
    void release() {
      out_links.release();
      in_links.release();
      memset(this, 0, sizeof(*this));
    }
  };
  Hash_Table<string_ref, u32> name2id;

  Array<Node>         nodes;
  Array<string_ref>   id2name;
  Array<Node_Wrapper> wrappers;

  Array<Link> links;

  Pool<char> string_storage;

  void init() {
    name2id.init();
    id2name.init();
    nodes.init();
    links.init();
    string_storage = Pool<char>::create(1 << 20);
  }
  void release() {
    name2id.release();
    id2name.release();
    nodes.release();
    links.release();
    string_storage.release();
  }
  void rebuild_index() {
    auto new_string_storage = Pool<char>::create(1 << 20);
    auto old_id2name        = id2name;
    id2name                 = Array<string_ref>();
    id2name.init();
    name2id.release();
    name2id.init();
    id2name.resize(nodes.size);
    ASSERT_DEBUG(nodes.size == wrappers.size);
    ito(nodes.size) {
      Node &node = nodes[i];
      if (node.is_alive()) {
        string_ref tmp_name = old_id2name[node.get_index()];
        // zero terminated
        char *name_ptr = new_string_storage.alloc(tmp_name.len + 1);
        memcpy(name_ptr, tmp_name.ptr, tmp_name.len + 1);
        string_ref new_name_ref = string_ref{.ptr = name_ptr, tmp_name.len};
        name2id.insert(new_name_ref, node.get_index());
        id2name[i]            = new_name_ref;
        wrappers[i].node_name = new_name_ref;
      }
    }
    string_storage.release();
    string_storage = new_string_storage;
    old_id2name.release();
  }
  string_ref get_name(u32 id) {
    ASSERT_DEBUG(id > 0);
    return id2name[id - 1];
  }
  void remove_node(string_ref name) {
    ASSERT_DEBUG(name2id.contains(name));
    u32 id = name2id.get(name);
    wrappers[id].release();
    nodes[id].release();
    name2id.remove(name);
  }
  u32 get_id(string_ref name) {
    if (name2id.contains(name)) {
      return name2id.get(name);
    }
    return 0;
  }
  u32 add_node(string_ref name, string_ref type_name) {
    Node_t type = str_to_node_type(type_name);
    if (type == Node_t::UNKNOWN) return 0;
    if (name2id.contains(name)) {
      PUSH_WARNING("Node name collision: %.*s", STRF(name));
      remove_node(name);
    }
    Node node;
    node.type   = type;
    node.pos.x  = 0.0f;
    node.pos.y  = 0.0f;
    node.size.x = 1.0f;
    node.size.y = 1.0f;
    node.id     = nodes.size + 1;
    nodes.push(node);
    wrappers.push({});
    {
      char *name_ptr = string_storage.try_alloc(name.len + 1);
      if (name_ptr == NULL) {
        rebuild_index();
      } else {
        memcpy(name_ptr, name.ptr, name.len);
        name_ptr[name.len]      = '\0';
        string_ref new_name_ref = string_ref{.ptr = name_ptr, name.len};
        name2id.insert(new_name_ref, node.get_index());
        id2name.push(new_name_ref);
        Node_Wrapper wrapper;
        wrapper.init();
        wrapper.node_id       = node.id;
        wrapper.node_name     = new_name_ref;
        wrappers[node.id - 1] = wrapper;
      }
    }

    return node.id;
  }
  void set_node_position(string_ref name, float x, float y) {
    if (name2id.contains(name)) {
      u32 id = name2id.get(name);
      set_node_position(id, x, y);
    }
  }
  void set_node_position(u32 id, float x, float y) {
    nodes[id - 1].pos.x = x;
    nodes[id - 1].pos.y = y;
  }
  void set_node_size(u32 id, float size_x, float size_y) {
    nodes[id - 1].size.x = size_x;
    nodes[id - 1].size.y = size_y;
  }
  u32 add_link(u32 src_node_id, u32 src_slot_id, u32 dst_node_id, u32 dst_slot_id) {
    ASSERT_RETNULL(src_node_id > 0 && src_node_id <= nodes.size);
    ASSERT_RETNULL(dst_node_id > 0 && dst_node_id <= nodes.size);
    Node &        src  = nodes[src_node_id];
//    Node_Wrapper &wsrc = wrappers[src_node_id];
    Node &        dst  = nodes[dst_node_id];
//    Node_Wrapper &wdst = wrappers[dst_node_id];
    ASSERT_RETNULL(src_slot_id > 0 && src_slot_id <= src.num_out_slots);
    ASSERT_RETNULL(dst_slot_id > 0 && dst_slot_id <= dst.num_in_slots);
    return 0;
  }
};

struct _Scene : public Scene {
  SourceDB sourcedb;
  NodeDB   nodedb;
  void     new_frame() { sourcedb.rebuild_index(); }
  void     init() {
    sourcedb.init();
    nodedb.init();
  }
  void release() { sourcedb.release(); }
  void reset() {
    release();
    init();
  }
  bool is_valid_name(char const *name) {
    bool valid = false;
    while (name[0] != '\0') {
      if (name[0] < 0x20 || name[0] > 0x7f || name[0] == '"') return false;
      valid = true;
      name++;
    }
    return valid;
  }
  void get_source_list(char const ***ptr, u32 *count) {
    ASSERT_DEBUG(count != NULL);
    ASSERT_DEBUG(ptr != NULL);
    *count = sourcedb.names_packed.size;
    *ptr   = sourcedb.names_packed.ptr;
  }
  char const *get_source(char const *name) { return sourcedb.get_text(stref_s(name)).ptr; }
  void        set_source(char const *name, char const *new_src) {
    sourcedb.update_text(stref_s(name), stref_s(new_src));
  }
  void remove_source(char const *name) { sourcedb.remove_source(stref_s(name)); }
  void add_source(char const *name, char const *text) {
    if (!is_valid_name(name)) {
      push_warning("Source's name is invalid");
      return;
    }
    sourcedb.add_source(stref_s(name), stref_s(text));
  }
  u32 add_node(char const *name, char const *type_name, float x, float y, float size_x,
               float size_y) {
    if (name == NULL || type_name == NULL) return 0;
    if (!is_valid_name(name)) {
      push_warning("Node's name is invalid");
      return 0;
    }
    u32 id = nodedb.add_node(stref_s(name), stref_s(type_name));
    ASSERT_RETNULL(id > 0);
    nodedb.set_node_position(id, x, y);
    nodedb.set_node_size(id, size_x, size_y);
    return id;
  }
  void run_script(char const *src_name) {
    string_ref source = sourcedb.get_text(stref_s(src_name));
    Evaluator  evaluator;
    evaluator.scene = this;
    evaluator.parse_and_eval(source);
  }
  struct Tmp_String_Builder {
    char * ptr;
    size_t size;
    size_t cursor;
    bool   error;
    void   init(size_t size) {
      ptr        = (char *)tl_alloc_tmp(size);
      error      = false;
      this->size = size;
      cursor     = 0;
    }
    void push_string(char const *str_raw) {
      string_ref str = stref_s(str_raw);
      char *     dst = ptr + cursor;
      cursor += str.len;
      if (cursor > size) error = true;
      memcpy(dst, str_raw, str.len);
    }
    void push_fmt(char const *fmt, ...) {
      char *  dst = ptr + cursor;
      va_list args;
      va_start(args, fmt);
      i32 num_chars = vsprintf(dst, fmt, args);
      va_end(args);
      if (num_chars > 0) {
        cursor += (size_t)num_chars;
      }
    }
    string_ref finish() { return string_ref{.ptr = ptr, .len = cursor}; }
  };
  string_ref get_save_script() {
    Tmp_String_Builder builder;
    builder.init(1 << 20);
    builder.push_string("(main\n");
    ito(nodedb.nodes.size) {
      Node &node = nodedb.nodes[i];
      if (node.is_alive()) {
        builder.push_fmt(                                   //
            "  (let node_%i (add_node \"%.*s\" \"%s\"))\n", //
            node.id,
            STRF(nodedb.get_name(node.id)), //
            node_type_to_str(node.type),    //
            0.0f, 0.0f,                     //
            1.0f, 1.0f);
        builder.push_fmt(                            //
            "  (set_node_position node_%i %f %f)\n", //
            node.id,                                 //
            node.pos.x, node.pos.y);
        builder.push_fmt(                        //
            "  (set_node_size node_%i %f %f)\n", //
            node.id,                             //
            node.size.x, node.size.y);
      }
    }
    ito(sourcedb.sources.size) {
      Source &src = sourcedb.sources[i];
      if (!src.is_alive()) continue;
      if (src.name == stref_s("init")) continue;
      builder.push_fmt(                                   //
          "  (add_source\n\"%.*s\"\n\"\"\"%.*s\"\"\")\n", //
          STRF(src.name),                                 //
          STRF(src.text)                                  //
      );
    }
    builder.push_fmt(                 //
        "  (move_camera %f %f %f)\n", //
        c2d.camera.pos.x, c2d.camera.pos.y, c2d.camera.pos.z);
    builder.push_string(")");
    return builder.finish();
  }
  struct Evaluator {
    _Scene *     scene;
    bool         eval_error;
    static char *get_msg_buf() {
      static char msg_buf[0x100] = {};
      return msg_buf;
    }
    static Pool<List> &get_list_storage() {
      static Pool<List> list_storage = Pool<List>::create((1 << 10));
      return list_storage;
    }
    struct Value {
      enum class Value_t { UNKNOWN = 0, I32, F32, SYMBOL };
      Value_t    type;
      f32        f;
      i32        i;
      string_ref str;
    };
    struct Symbol {
      string_ref name;
      Value *    val;
    };
    static Pool<Symbol> &get_symbol_table() {
      static Pool<Symbol> symbol_table = Pool<Symbol>::create((1 << 10));
      return symbol_table;
    }
    void enter_scope() { get_symbol_table().enter_scope(); }
    void exit_scope() { get_symbol_table().exit_scope(); }
    void add_symbol(string_ref name, Value *val) {
      get_symbol_table().push({.name = name, .val = val});
    }
    Value *lookup_symbol(string_ref name) {
      ito(get_symbol_table().cursor) {
        if (get_symbol_table().at(i)->name == name) {
          return get_symbol_table().at(i)->val;
        }
      }
      return NULL;
    }
    void parse_and_eval(string_ref source) {
      get_list_storage().enter_scope();
      defer(get_list_storage().exit_scope());
      struct List_Allocator {
        List *alloc() { return get_list_storage().alloc_zero(1); }
        void  reset() {}
      } list_allocator;
      List *root = List::parse(source, list_allocator);
      if (root != NULL) {
        TMP_STORAGE_SCOPE;
        eval_error = false;
        eval(root);
        if (eval_error) {
          scene->push_warning("Evaluation error");
        }
      } else {
        scene->push_warning("Parse error");
      }
    }
    Value *eval(List *l) {
      if (l == NULL) return NULL;
        ///////////////////
        // Macro helpers //
        ///////////////////
#define ALLOC_VAL() (Value *)tl_alloc_tmp(sizeof(Value))
#define EVAL_ASSERT(x)                                                                             \
  do {                                                                                             \
    if (!(x)) {                                                                                    \
      eval_error = true;                                                                           \
      scene->push_error(#x);                                                                       \
      return NULL;                                                                                 \
    }                                                                                              \
  } while (0)
#define CHECK_ERROR                                                                                \
  do {                                                                                             \
    if (eval_error) {                                                                              \
      return NULL;                                                                                 \
    }                                                                                              \
  } while (0)
#define CALL_EVAL(x)                                                                               \
  eval(x);                                                                                         \
  CHECK_ERROR
#define ASSERT_SMB(x) EVAL_ASSERT(x != NULL && x->type == Value::Value_t::SYMBOL);
#define ASSERT_I32(x) EVAL_ASSERT(x != NULL && x->type == Value::Value_t::I32);
#define ASSERT_F32(x) EVAL_ASSERT(x != NULL && x->type == Value::Value_t::F32);
#define EVAL_SMB(res, id)                                                                          \
  Value *res = CALL_EVAL(l->get(id));                                                              \
  ASSERT_SMB(res)
#define EVAL_I32(res, id)                                                                          \
  Value *res = CALL_EVAL(l->get(id));                                                              \
  ASSERT_I32(res)
#define EVAL_F32(res, id)                                                                          \
  Value *res = CALL_EVAL(l->get(id));                                                              \
  ASSERT_F32(res)
      ///////////////////
      if (l->nonempty()) {
        i32  imm32;
        f32  immf32;
        bool is_imm32  = parse_decimal_int(l->symbol.ptr, l->symbol.len, &imm32);
        bool is_immf32 = parse_float(l->symbol.ptr, l->symbol.len, &immf32);
        if (is_imm32) {
          Value *new_val = ALLOC_VAL();
          new_val->i     = imm32;
          new_val->type  = Value::Value_t::I32;
          return new_val;
        } else if (is_immf32) {
          Value *new_val = ALLOC_VAL();
          new_val->f     = immf32;
          new_val->type  = Value::Value_t::F32;
          return new_val;
        } else if (l->cmp_symbol("main")) {
          enter_scope();
          defer(exit_scope());
          List *cur = l->next;
          while (cur != NULL) {
            CALL_EVAL(cur);
            cur = cur->next;
          }
          return NULL;
        } else if (l->cmp_symbol("add_node")) {
          EVAL_SMB(name, 1);
          EVAL_SMB(type, 2);
          u32    id      = scene->nodedb.add_node(name->str, type->str);
          Value *new_val = ALLOC_VAL();
          new_val->i     = id;
          new_val->type  = Value::Value_t::I32;
          return new_val;
        } else if (l->cmp_symbol("set_node_position")) {
          EVAL_I32(id, 1);
          EVAL_F32(x, 2);
          EVAL_F32(y, 3);
          scene->nodedb.set_node_position(id->i, x->f, y->f);
          return NULL;
        } else if (l->cmp_symbol("set_node_size")) {
          EVAL_I32(id, 1);
          EVAL_F32(x, 2);
          EVAL_F32(y, 3);
          scene->nodedb.set_node_size(id->i, x->f, y->f);
          return NULL;
        } else if (l->cmp_symbol("itof")) {
          EVAL_I32(a, 1);
          Value *new_val = ALLOC_VAL();
          new_val->f     = (float)a->i;
          new_val->type  = Value::Value_t::F32;
          return new_val;
        } else if (l->cmp_symbol("add")) {
          Value *op1 = CALL_EVAL(l->get(1));
          EVAL_ASSERT(op1 != NULL);
          Value *op2 = CALL_EVAL(l->get(2));
          EVAL_ASSERT(op2 != NULL);
          EVAL_ASSERT(op1->type == op2->type);
          if (op1->type == Value::Value_t::I32) {
            Value *new_val = ALLOC_VAL();
            new_val->i     = op1->i + op2->i;
            new_val->type  = Value::Value_t::I32;
            return new_val;
          } else if (op1->type == Value::Value_t::F32) {
            Value *new_val = ALLOC_VAL();
            new_val->f     = op1->f + op2->f;
            new_val->type  = Value::Value_t::F32;
            return new_val;
          } else {
            scene->push_warning("add: unsopported operand types");
            eval_error = true;
          }
          return NULL;
        }  else if (l->cmp_symbol("mul")) {
          Value *op1 = CALL_EVAL(l->get(1));
          EVAL_ASSERT(op1 != NULL);
          Value *op2 = CALL_EVAL(l->get(2));
          EVAL_ASSERT(op2 != NULL);
          EVAL_ASSERT(op1->type == op2->type);
          if (op1->type == Value::Value_t::I32) {
            Value *new_val = ALLOC_VAL();
            new_val->i     = op1->i * op2->i;
            new_val->type  = Value::Value_t::I32;
            return new_val;
          } else if (op1->type == Value::Value_t::F32) {
            Value *new_val = ALLOC_VAL();
            new_val->f     = op1->f * op2->f;
            new_val->type  = Value::Value_t::F32;
            return new_val;
          } else {
            scene->push_warning("mul: unsopported operand types");
            eval_error = true;
          }
          return NULL;
        } else if (l->cmp_symbol("add_source")) {
          Value *name = CALL_EVAL(l->get(1));
          EVAL_ASSERT(name != NULL && name->type == Value::Value_t::SYMBOL);
          Value *text = CALL_EVAL(l->get(2));
          EVAL_ASSERT(text != NULL && text->type == Value::Value_t::SYMBOL);
          scene->add_source(stref_to_tmp_cstr(name->str), stref_to_tmp_cstr(text->str));
          return NULL;
        } else if (l->cmp_symbol("for")) {
          Value *name = CALL_EVAL(l->get(1));
          EVAL_ASSERT(name != NULL && name->type == Value::Value_t::SYMBOL);
          Value *lb = CALL_EVAL(l->get(2));
          EVAL_ASSERT(lb != NULL && lb->type == Value::Value_t::I32);
          Value *ub = CALL_EVAL(l->get(3));
          EVAL_ASSERT(ub != NULL && ub->type == Value::Value_t::I32);
          Value *new_val = ALLOC_VAL();
          new_val->i     = 0;
          new_val->type  = Value::Value_t::I32;
          for (i32 i = lb->i; i < ub->i; i++) {
            enter_scope();
            new_val->i = i;
            add_symbol(name->str, new_val);
            defer(exit_scope());
            List *cur = l->get(4);
            while (cur != NULL) {
              CALL_EVAL(cur);
              cur = cur->next;
            }
          }
          return NULL;
        } else if (l->cmp_symbol("get_num_nodes")) {
          Value *new_val = ALLOC_VAL();
          new_val->i     = (i32)scene->nodedb.nodes.size;
          new_val->type  = Value::Value_t::I32;
          return new_val;
        } else if (l->cmp_symbol("is_node_alive")) {
          Value *new_val = ALLOC_VAL();
          Value *index   = CALL_EVAL(l->get(1));
          EVAL_ASSERT(index != NULL && index->type == Value::Value_t::I32);
          new_val->i    = (scene->nodedb.nodes[index->i - 1].is_alive() ? 1 : 0);
          new_val->type = Value::Value_t::I32;
          return new_val;
        } else if (l->cmp_symbol("print")) {
          Value *str = CALL_EVAL(l->get(1));
          EVAL_ASSERT(str != NULL && str->type == Value::Value_t::SYMBOL);
          scene->push_debug_message("%.*s", STRF(str->str));
          return NULL;
        } else if (l->cmp_symbol("let")) {
          Value *name = CALL_EVAL(l->get(1));
          EVAL_ASSERT(name != NULL && name->type == Value::Value_t::SYMBOL);
          Value *val = CALL_EVAL(l->get(2));
          EVAL_ASSERT(val != NULL);
          add_symbol(name->str, val);
          return NULL;
        } else if (l->cmp_symbol("move_camera")) {
          Value *x = CALL_EVAL(l->get(1));
          EVAL_ASSERT(x != NULL && x->type == Value::Value_t::F32);
          Value *y = CALL_EVAL(l->get(2));
          EVAL_ASSERT(y != NULL && y->type == Value::Value_t::F32);
          Value *z = CALL_EVAL(l->get(3));
          EVAL_ASSERT(z != NULL && z->type == Value::Value_t::F32);
          scene->c2d.camera.pos.x = x->f;
          scene->c2d.camera.pos.y = y->f;
          scene->c2d.camera.pos.z = z->f;
          return NULL;
        } else if (l->cmp_symbol("format")) {
          Value *fmt = CALL_EVAL(l->get(1));
          EVAL_ASSERT(fmt != NULL && fmt->type == Value::Value_t::SYMBOL);
          List *cur = l->get(2);
          {
            char *      tmp_buf = (char *)tl_alloc_tmp(0x100);
            u32         cursor  = 0;
            char const *c       = fmt->str.ptr;
            char const *end     = fmt->str.ptr + fmt->str.len;
            while (c != end) {
              if (c[0] == '%') {
                if (c + 1 == end) {
                  eval_error = true;
                  scene->push_error("[format] Format string ends with %");
                  return NULL;
                }

                if (cur == NULL) {
                  eval_error = true;
                  scene->push_error("[format] Not enough arguments", c[1]);
                  return NULL;
                } else {
                  i32    num_chars = 0;
                  Value *val       = eval(cur);
                  if (c[1] == 'i') {
                    EVAL_ASSERT(val != NULL && val->type == Value::Value_t::I32);
                    num_chars = sprintf(tmp_buf + cursor, "%i", val->i);
                  } else if (c[1] == 'f') {
                    EVAL_ASSERT(val != NULL && val->type == Value::Value_t::F32);
                    num_chars = sprintf(tmp_buf + cursor, "%f", val->f);
                  } else if (c[1] == 's') {
                    EVAL_ASSERT(val != NULL && val->type == Value::Value_t::SYMBOL);
                    num_chars = sprintf(tmp_buf + cursor, "%.*s", (i32)val->str.len, val->str.ptr);
                  } else {
                    eval_error = true;
                    scene->push_error("[format] Unknown format: %%%c", c[1]);
                    return NULL;
                  }
                  if (num_chars < 0) {
                    eval_error = true;
                    scene->push_error("[format] Blimey!");
                    return NULL;
                  }
                  if (num_chars > 0x100) {
                    eval_error = true;
                    scene->push_error("[format] Format buffer overflow!");
                    return NULL;
                  }
                  cursor += num_chars;
                }
                cur = cur->next;
                c += 1;
              } else {
                tmp_buf[cursor++] = c[0];
              }
              c += 1;
            }
            Value *new_val = ALLOC_VAL();
            new_val->str   = stref_s(tmp_buf);
            new_val->type  = Value::Value_t::SYMBOL;
            return new_val;
          }
        } else {
          EVAL_ASSERT(l->nonempty());
          Value *sym = lookup_symbol(l->symbol);
          if (sym != NULL) {
            return sym;
          }
          Value *new_val = ALLOC_VAL();
          new_val->str   = l->symbol;
          new_val->type  = Value::Value_t::SYMBOL;
          return new_val;
        }
      } else {
        EVAL_ASSERT(l->child != NULL);
        Value *child_value = CALL_EVAL(l->child);
        return child_value;
      }
      TRAP;
#undef EVAL_ASSERT
#undef CHECK_ERROR
    }
  };

  void consume_event(SDL_Event event) {
    static bool  ldown             = false;
    static int   old_mp_x          = 0;
    static int   old_mp_y          = 0;
    static bool  hyper_pressed     = false;
    static bool  skip_key          = false;
    static bool  lctrl             = false;
    static float old_mouse_world_x = 0.0f;
    static float old_mouse_world_y = 0.0f;
    static i32   selected_node     = -1;
    switch (event.type) {
    case SDL_QUIT: {
      break;
    }
    case SDL_KEYUP: {
      if (event.key.keysym.sym == SDLK_LCTRL) {
        lctrl = false;
      }
      break;
    }
    case SDL_TEXTINPUT: {
    } break;
    case SDL_KEYDOWN: {
      uint32_t c = event.key.keysym.sym;
      (void)c;
      switch (event.key.keysym.sym) {

      case SDLK_ESCAPE: {
        break;
      }
      }
      break;
    }
    case SDL_MOUSEBUTTONDOWN: {
      SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)&event;
      if (m->button == 3) {
      }
      if (m->button == 1) {
        ldown = true;
      }
      ito(nodedb.nodes.size) {
        Node &node = nodedb.nodes[i];
        if (node.is_alive() && node.inside(c2d.camera.mouse_world_x, c2d.camera.mouse_world_y)) {
          selected_node = i;
          return;
        }
      }
      selected_node = -1;
      if (c2d.hovered) c2d.consume_event(event);
      break;
    }
    case SDL_MOUSEBUTTONUP: {
      SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)&event;
      selected_node           = -1;
      if (m->button == 1) ldown = false;
      if (c2d.hovered) c2d.consume_event(event);
      break;
    }
    case SDL_WINDOWEVENT_FOCUS_LOST: {
      skip_key      = false;
      hyper_pressed = false;
      ldown         = false;
      lctrl         = false;
      if (c2d.hovered) c2d.consume_event(event);
      break;
    }
    case SDL_MOUSEMOTION: {
      SDL_MouseMotionEvent *m = (SDL_MouseMotionEvent *)&event;
      if (c2d.hovered) c2d.consume_event(event);
//      int    dx = m->x - old_mp_x;
//      int    dy = m->y - old_mp_y;
//      float2 wd = c2d.camera.window_to_world({m->x, m->y}) -
//                  c2d.camera.window_to_world({old_mp_x, old_mp_y});
      old_mp_x          = m->x;
      old_mp_y          = m->y;
      float wmdx        = c2d.camera.mouse_world_x - old_mouse_world_x;
      float wmdy        = c2d.camera.mouse_world_y - old_mouse_world_y;
      old_mouse_world_x = c2d.camera.mouse_world_x;
      old_mouse_world_y = c2d.camera.mouse_world_y;

      if (ldown && selected_node >= 0) {
        Node &node = nodedb.nodes[selected_node];
        node.pos.x += wmdx; // c2d.camera.pos.z * (float)dx / c2d.camera.viewport_height;
        node.pos.y += wmdy; // c2d.camera.pos.z * (float)dy / c2d.camera.viewport_height;
      }

    } break;
    case SDL_MOUSEWHEEL: {
      if (c2d.hovered) c2d.consume_event(event);
    } break;
    }
  }
};

void Scene::draw() {
  _Scene *scene = (_Scene *)this;
  scene->new_frame();
  line_storage.enter_scope();
  quad_storage.enter_scope();
  string_storage.enter_scope();
  char_storage.enter_scope();
  TMP_STORAGE_SCOPE
  c2d.imcanvas_start();
  defer({ c2d.imcanvas_end(); });
  u32    W               = 256;
  u32    H               = 256;
  float  dx              = 1.0f;
  float  dy              = 1.0f;
  float  size_x          = 256.0f;
  float  size_y          = 256.0f;
  float  GRID_LAYER      = 2.0f / 256.0f;
  float  NODE_BG_LAYER   = 10.0f / 256.0f;
  float  NODE_NAME_LAYER = 11.0f / 256.0f;
  float3 grid_color      = parse_color_float3(dark_mode::g_grid_color);
  //  c2d.draw_string({//
  //                   .c_str = "hello world!",
  //                   .x     = c2d.camera.mouse_world_x,
  //                   .y     = c2d.camera.mouse_world_y,
  //                   .z     = GRID_LAYER,
  //                   .color = {.r = 1.0f, .g = 1.0f, .b = 1.0f}});
  c2d.draw_rect({//
                 .x      = c2d.camera.mouse_world_x,
                 .y      = c2d.camera.mouse_world_y,
                 .z      = GRID_LAYER,
                 .width  = 1.0e-1f,
                 .height = 1.0e-1f,

                 .color = {.r = grid_color.x, .g = grid_color.y, .b = grid_color.z}});
  ito(W + 1) {
    c2d.draw_line({//
                   .x0    = dx * (float)(i),
                   .y0    = 0.0f,
                   .x1    = dx * (float)(i),
                   .y1    = size_y,
                   .z     = GRID_LAYER,
                   .color = {.r = grid_color.x, .g = grid_color.y, .b = grid_color.z}});
  }
  ito(H + 1) {
    c2d.draw_line({//
                   .x0    = 0.0f,
                   .y0    = dy * (float)(i),
                   .x1    = size_x,
                   .y1    = dy * (float)(i),
                   .z     = GRID_LAYER,
                   .color = {.r = grid_color.x, .g = grid_color.y, .b = grid_color.z}});
  }
  ito(scene->nodedb.nodes.size) {
    Node &node = scene->nodedb.nodes[i];
    if (node.is_alive()) {
      c2d.draw_string({//
                       .c_str = scene->nodedb.get_name(node.id).ptr,
                       .x     = node.pos.x,
                       .y     = node.pos.y,
                       .z     = NODE_NAME_LAYER,
                       .color = {.r = 1.0f, .g = 1.0f, .b = 1.0f}});
      c2d.draw_rect({//
                     .x      = node.pos.x - node.size.x,
                     .y      = node.pos.y - node.size.y,
                     .z      = NODE_BG_LAYER,
                     .width  = node.size.x * 2.0f,
                     .height = node.size.y * 2.0f,
                     .color  = {.r = 0.5f, .g = 0.5f, .b = 0.5f}});
    }
  }
}

u32 Scene::add_node(char const *name, char const *type, float x, float y, float size_x,
                    float size_y) {
  _Scene *scene = (_Scene *)this;
  return scene->add_node(name, type, x, y, size_x, size_y);
}
void Scene::get_source_list(char const ***ptr, u32 *count) {
  _Scene *scene = (_Scene *)this;
  scene->get_source_list(ptr, count);
}
void Scene::get_node_type_list(char const ***ptr, u32 *count) {
  *ptr   = Node_Type_Name_Table;
  *count = ARRAY_SIZE(Node_Type_Name_Table) - 1;
}
char const *Scene::get_source(char const *name) {
  _Scene *scene = (_Scene *)this;
  return scene->get_source(name);
}
void Scene::set_source(char const *name, char const *new_src) {
  _Scene *scene = (_Scene *)this;
  scene->set_source(name, new_src);
}
void Scene::remove_source(char const *name) {
  _Scene *scene = (_Scene *)this;

  scene->remove_source(name);
}
void Scene::add_source(char const *name, char const *text) {
  _Scene *scene = (_Scene *)this;
  scene->add_source(name, text);
}
void Scene::reset() {
  _Scene *scene = (_Scene *)this;
  scene->reset();
}
void Scene::run_script(char const *src_name) {
  _Scene *scene = (_Scene *)this;
  scene->run_script(src_name);
}
string_ref Scene::get_save_script() {
  _Scene *scene = (_Scene *)this;
  return scene->get_save_script();
}
_Scene     g_scene;
static int _init_ = [] {
  g_scene.init();
  return 0;
}();

Scene *Scene::get_scene() { return &g_scene; }

void Context2D::flush_rendering() {
  render_stuff();
  line_storage.exit_scope();
  quad_storage.exit_scope();
  string_storage.exit_scope();
  char_storage.exit_scope();
}
void Context2D::draw_rect(Rect2D p) { quad_storage.push(p); }
void Context2D::draw_line(Line2D l) { line_storage.push(l); }
void Context2D::draw_string(String2D s) {
  size_t len = strlen(s.c_str);
  if (len == 0) return;
  char *dst = char_storage.alloc(len + 1);
  memcpy(dst, s.c_str, len);
  dst[len] = '\0';
  _String2D internal_string;
  internal_string.color       = s.color;
  internal_string.c_str       = dst;
  internal_string.len         = (uint32_t)len;
  internal_string.x           = s.x;
  internal_string.y           = s.y;
  internal_string.z           = s.z;
  internal_string.world_space = s.world_space;

  string_storage.push(internal_string);
}

void Oth_Camera::update(u32 viewport_x, u32 viewport_y, u32 viewport_width, u32 viewport_height) {
  this->viewport_x      = viewport_x;
  this->viewport_width  = viewport_width;
  this->viewport_y      = viewport_y;
  this->viewport_height = viewport_height;
  height_over_width     = ((float)viewport_height / viewport_width);
  width_over_heigth     = ((float)viewport_width / viewport_height);
  float e               = (float)2.4e-7f;
  fovy                  = 2.0f;
  fovx                  = 2.0f * height_over_width;
  (void)e;
  // clang-format off
      float4 proj[4] = {
        {fovx/pos.z,   0.0f,          0.0f,      -fovx * pos.x/pos.z},
        {0.0f,         fovy/pos.z,    0.0f,      -fovy * pos.y/pos.z},
        {0.0f,         0.0f,          1.0f,      0.0f},
        {0.0f,         0.0f,          0.0f,      1.0f}
      };
  // clang-format on
  memcpy(&this->proj[0], &proj[0], sizeof(proj));
  world_min_x = screen_to_world(float2{-1.0f, -1.0f}).x;
  world_min_y = screen_to_world(float2{-1.0f, -1.0f}).y;
  world_max_x = screen_to_world(float2{1.0f, 1.0f}).x;
  world_max_y = screen_to_world(float2{1.0f, 1.0f}).y;
  glyphs_world_height =
      glyph_scale * (float)(simplefont_bitmap_glyphs_height) / viewport_height * pos.z;
  glyphs_world_width =
      glyph_scale * (float)(simplefont_bitmap_glyphs_width) / viewport_width * pos.z;
  pixel_screen_width  = 2.0f / viewport_width;
  pixel_screen_height = 2.0f / viewport_height;
  glyphs_screen_width =
      2.0f * glyph_scale * (float)(simplefont_bitmap_glyphs_width) / viewport_width;
  glyphs_screen_height =
      2.0f * glyph_scale * (float)(simplefont_bitmap_glyphs_height) / viewport_height;
}

void compile_shader(GLuint shader) {
  glCompileShader(shader);
  GLint isCompiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
  if (isCompiled == GL_FALSE) {
    GLint maxLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
    GLchar *errorLog = (GLchar *)malloc(maxLength);
    defer(free(errorLog));
    glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);

    glDeleteShader(shader);
    fprintf(stderr, "[ERROR]: %s\n", &errorLog[0]);
    exit(1);
  }
}

void link_program(GLuint program) {
  glLinkProgram(program);
  GLint isLinked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
  if (isLinked == GL_FALSE) {
    GLint maxLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
    GLchar *infoLog = (GLchar *)malloc(maxLength);
    defer(free(infoLog));
    glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
    glDeleteProgram(program);
    fprintf(stderr, "[ERROR]: %s\n", &infoLog[0]);
    exit(1);
  }
}

GLuint create_program(GLchar const *vsrc, GLchar const *fsrc) {
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vsrc, NULL);
  compile_shader(vertexShader);
  defer(glDeleteShader(vertexShader););
  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fsrc, NULL);
  compile_shader(fragmentShader);
  defer(glDeleteShader(fragmentShader););

  GLuint program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  link_program(program);
  glDetachShader(program, vertexShader);
  glDetachShader(program, fragmentShader);

  return program;
}

void Oth_Camera::consume_event(SDL_Event event) {
  static bool ldown         = false;
  static int  old_mp_x      = 0;
  static int  old_mp_y      = 0;
  static bool hyper_pressed = false;
  static bool skip_key      = false;
  static bool lctrl         = false;
  switch (event.type) {
  case SDL_QUIT: {
    break;
  }
  case SDL_KEYUP: {
    if (event.key.keysym.sym == SDLK_LCTRL) {
      lctrl = false;
    }
    break;
  }
  case SDL_TEXTINPUT: {
  } break;
  case SDL_KEYDOWN: {
    uint32_t c = event.key.keysym.sym;
    (void)c;
    switch (event.key.keysym.sym) {

    case SDLK_ESCAPE: {
      break;
    }
    }
    break;
  }
  case SDL_MOUSEBUTTONDOWN: {
    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)&event;
    if (m->button == 3) {
    }
    if (m->button == 1) {

      ldown = true;
    }
    break;
  }
  case SDL_MOUSEBUTTONUP: {
    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)&event;
    if (m->button == 1) ldown = false;
    break;
  }
  case SDL_WINDOWEVENT_FOCUS_LOST: {
    skip_key      = false;
    hyper_pressed = false;
    ldown         = false;
    lctrl         = false;
    break;
  }
  case SDL_MOUSEMOTION: {
    SDL_MouseMotionEvent *m = (SDL_MouseMotionEvent *)&event;

    int dx = m->x - old_mp_x;
    int dy = m->y - old_mp_y;
    if (ldown) {
      pos.x -= pos.z * (float)dx / viewport_height;
      pos.y += pos.z * (float)dy / viewport_height;
    }
    mouse_screen_x = 2.0f * (float(m->x - viewport_x) + 0.5f) / viewport_width - 1.0f;
    mouse_screen_y = -2.0f * (float(m->y - viewport_y) - 0.5f) / viewport_height + 1.0f;
    float2 mwp     = screen_to_world(float2{mouse_screen_x, mouse_screen_y});
    mouse_world_x  = mwp.x;
    mouse_world_y  = mwp.y;
    old_mp_x       = m->x;
    old_mp_y       = m->y;
  } break;
  case SDL_MOUSEWHEEL: {
    float dz = pos.z * (float)(event.wheel.y > 0 ? 1 : -1) * 2.0e-1;
    //    fprintf(stdout, "dz: %f\n", dz);
    pos.z += dz;
    pos.z = clamp(pos.z, 0.1f, 512.0f);
    pos.x += -0.5f * dz * (window_to_screen(int2{(int32_t)old_mp_x, 0}).x);
    pos.y += -0.5f * dz * (window_to_screen(int2{0, (int32_t)old_mp_y}).y);

  } break;
  }
}

void Context2D::consume_event(SDL_Event event) { camera.consume_event(event); }

void Scene::consume_event(SDL_Event event) {
  _Scene *scene = (_Scene *)this;
  scene->consume_event(event);
}

void Context2D::render_stuff() {
  glViewport((float)viewport_x, (float)(screen_height - viewport_y - viewport_height),
             (float)viewport_width, (float)viewport_height);
  glScissor(viewport_x, screen_height - viewport_y - viewport_height, viewport_width,
            viewport_height);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_SCISSOR_TEST);
  glDepthMask(GL_TRUE);
  glDepthFunc(GL_GEQUAL);
  glDisable(GL_CULL_FACE);
  float3 bg_color = parse_color_float3(dark_mode::g_background_color);
  glClearColor(bg_color.x, bg_color.y, bg_color.z, 1.0f);
  glClearDepthf(0.0f);
  glLineWidth(1.0f);
  {
    const GLchar *line_vs =
        R"(#version 300 es
        precision highp float;
  layout (location = 0) in vec4 vertex_position;
  layout (location = 1) in vec3 vertex_color;
  uniform mat4 projection;
  out vec3 color;
  void main() {
      color = vertex_color;
      if (vertex_position.w > 0.0)
        gl_Position = vertex_position * projection;
      else
        gl_Position = vec4(vertex_position.xyz, 1.0);
  })";
    const GLchar *line_ps =
        R"(#version 300 es
        precision highp float;
  layout(location = 0) out vec4 SV_TARGET0;
  in vec3 color;
  void main() {
    SV_TARGET0 = vec4(color, 1.0);
  })";
    const GLchar *quad_vs =
        R"(#version 300 es
        precision highp float;
  layout (location = 0) in vec2 vertex_position;
  layout (location = 1) in vec4 instance_offset;
  layout (location = 2) in vec3 instance_color;
  layout (location = 3) in vec2 instance_size;

  out vec3 color;
  uniform mat4 projection;
  void main() {
      color = instance_color;
      if (instance_offset.w > 0.0)
        gl_Position =  vec4(vertex_position * instance_size + instance_offset.xy, instance_offset.z, 1.0) * projection;
      else
        gl_Position =  vec4(vertex_position * instance_size + instance_offset.xy, instance_offset.z, 1.0);
  })";
    const GLchar *quad_ps =
        R"(#version 300 es
        precision highp float;
  layout(location = 0) out vec4 SV_TARGET0;
  in vec3 color;
  void main() {
    SV_TARGET0 = vec4(color, 1.0);
  })";
    static GLuint line_program = create_program(line_vs, line_ps);
    static GLuint quad_program = create_program(quad_vs, quad_ps);
    static GLuint line_vao;
    static GLuint line_vbo;
    static GLuint quad_vao;
    static GLuint quad_vbo;
    static GLuint quad_instance_vbo;
    static int    init_va0 = [&] {
      glGenVertexArrays(1, &line_vao);
      glBindVertexArray(line_vao);
      glGenBuffers(1, &line_vbo);
      glBindBuffer(GL_ARRAY_BUFFER, line_vbo);

      glGenVertexArrays(1, &quad_vao);
      glBindVertexArray(quad_vao);
      float pos[] = {
          0.0f, 0.0f, //
          1.0f, 0.0f, //
          1.0f, 1.0f, //
          0.0f, 0.0f, //
          1.0f, 1.0f, //
          0.0f, 1.0f, //
      };
      glGenBuffers(1, &quad_vbo);
      glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_DYNAMIC_DRAW);

      glGenBuffers(1, &quad_instance_vbo);
      glBindBuffer(GL_ARRAY_BUFFER, quad_instance_vbo);

      glBindVertexArray(0);
      glBindBuffer(GL_ARRAY_BUFFER, 0);

      return 0;
    }();
    (void)init_va0;
    // Draw quads
    if (quad_storage.cursor != 0) {
      glUseProgram(quad_program);
      glUniformMatrix4fv(glGetUniformLocation(quad_program, "projection"), 1, GL_FALSE,
                         (float *)&camera.proj[0]);
      glBindVertexArray(quad_vao);
      glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);
      glEnableVertexAttribArray(0);
      // glVertexAttribBinding(0, 0);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

      glBindBuffer(GL_ARRAY_BUFFER, quad_instance_vbo);
      struct Rect_Instance_GL {
        float x, y, z, w;
        float r, g, b;
        float width, height;
      };
      static_assert(sizeof(Rect_Instance_GL) == 36, "");
      //      uint32_t max_num_quads = width * height;
      uint32_t max_num_quads = quad_storage.cursor;
      uint32_t num_quads     = 0;
      TMP_STORAGE_SCOPE;
      Rect_Instance_GL *qinstances =
          (Rect_Instance_GL *)tl_alloc_tmp(sizeof(Rect_Instance_GL) * max_num_quads);
      ito(max_num_quads) {
        Rect2D quad2d = *quad_storage.at(i);
        if (quad2d.world_space && !camera.intersects(quad2d.x, quad2d.y, quad2d.x + quad2d.width,
                                                     quad2d.y + quad2d.height))
          continue;
        Rect_Instance_GL quadgl;
        if (quad2d.world_space) {
          quadgl.x      = quad2d.x;
          quadgl.y      = quad2d.y;
          quadgl.z      = quad2d.z;
          quadgl.w      = 1.0f;
          quadgl.width  = quad2d.width;
          quadgl.height = quad2d.height;
        } else {
          quadgl.x      = 2.0f * quad2d.x / viewport_width - 1.0f;
          quadgl.y      = -2.0f * quad2d.y / viewport_height + 1.0f;
          quadgl.z      = quad2d.z;
          quadgl.w      = 0.0f;
          quadgl.width  = 2.0f * quad2d.width / viewport_width;
          quadgl.height = -2.0f * quad2d.height / viewport_height;
        }

        quadgl.r                = quad2d.color.r;
        quadgl.g                = quad2d.color.g;
        quadgl.b                = quad2d.color.b;
        qinstances[num_quads++] = quadgl;
      }
      if (num_quads == 0) goto skip_quads;
      glBufferData(GL_ARRAY_BUFFER, sizeof(Rect_Instance_GL) * num_quads, qinstances,
                   GL_DYNAMIC_DRAW);

      glEnableVertexAttribArray(1);
      // glVertexAttribBinding(1, 0);
      glVertexAttribDivisor(1, 1);
      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Rect_Instance_GL), 0);
      glEnableVertexAttribArray(2);
      // glVertexAttribBinding(2, 0);
      glVertexAttribDivisor(2, 1);
      glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Rect_Instance_GL), (void *)16);
      glEnableVertexAttribArray(3);
      // glVertexAttribBinding(3, 0);
      glVertexAttribDivisor(3, 1);
      glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Rect_Instance_GL), (void *)28);

      glDrawArraysInstanced(GL_TRIANGLES, 0, 6, num_quads);

      glBindVertexArray(0);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
      glDisableVertexAttribArray(2);
      glVertexAttribDivisor(1, 0);
      glVertexAttribDivisor(2, 0);
    }
  skip_quads:
    // Draw lines
    if (line_storage.cursor != 0) {
      uint32_t num_lines = line_storage.cursor;
      struct Line_GL {
        float x0, y0, z0, w0;
        float r0, g0, b0;
        float x1, y1, z1, w1;
        float r1, g1, b1;
      };
      static_assert(sizeof(Line_GL) == 56, "");
      TMP_STORAGE_SCOPE;
      Line_GL *lines = (Line_GL *)tl_alloc_tmp(sizeof(Line_GL) * num_lines);
      ito(num_lines) {
        Line2D  l = *line_storage.at(i);
        Line_GL lgl;
        if (l.world_space) {
          lgl.x0 = l.x0;
          lgl.y0 = l.y0;
          lgl.z0 = l.z;
          lgl.w0 = 1.0f;
          lgl.r0 = l.color.r;
          lgl.g0 = l.color.g;
          lgl.b0 = l.color.b;
          lgl.x1 = l.x1;
          lgl.y1 = l.y1;
          lgl.z1 = l.z;
          lgl.w1 = 1.0f;
        } else {
          lgl.x0 = 2.0f * l.x0 / viewport_width - 1.0f;
          lgl.y0 = -2.0f * l.y0 / viewport_height + 1.0f;
          lgl.z0 = l.z;
          lgl.w0 = 0.0f;
          lgl.r0 = l.color.r;
          lgl.g0 = l.color.g;
          lgl.b0 = l.color.b;
          lgl.x1 = 2.0f * l.x1 / viewport_width - 1.0f;
          lgl.y1 = -2.0f * l.y1 / viewport_height + 1.0f;
          lgl.z1 = l.z;
          lgl.w1 = 0.0f;
        }

        lgl.r1   = l.color.r;
        lgl.g1   = l.color.g;
        lgl.b1   = l.color.b;
        lines[i] = lgl;
      }
      glBindVertexArray(line_vao);
      glBindBuffer(GL_ARRAY_BUFFER, line_vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(Line_GL) * num_lines, lines, GL_DYNAMIC_DRAW);
      glUseProgram(line_program);
      glUniformMatrix4fv(glGetUniformLocation(line_program, "projection"), 1, GL_FALSE,
                         (float *)&camera.proj[0]);

      glEnableVertexAttribArray(0);
      // glVertexAttribBinding(0, 0);
      glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 28, 0);
      glEnableVertexAttribArray(1);
      // glVertexAttribBinding(1, 0);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 28, (void *)16);
      glDrawArrays(GL_LINES, 0, 2 * num_lines);

      glBindVertexArray(0);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
    }
    // Draw strings
    if (string_storage.cursor != 0) {
      const GLchar *vsrc =
          R"(#version 300 es
          precision highp float;
  layout (location=0) in vec2 vertex_position;
  layout (location=1) in vec3 instance_offset;
  layout (location=2) in vec2 instance_uv_offset;
  layout (location=3) in vec3 instance_color;

  out vec2 uv;
  out vec3 color;

  uniform vec2 glyph_uv_size;
  uniform vec2 glyph_size;
  uniform vec2 viewport_size;
  void main() {
      color = instance_color;
      uv = instance_uv_offset + (vertex_position * vec2(1.0, -1.0) + vec2(0.0, 1.0)) * glyph_uv_size;
      vec4 sspos =  vec4(vertex_position * glyph_size + instance_offset.xy, 0.0, 1.0);
      int pixel_x = int(viewport_size.x * (sspos.x * 0.5 + 0.5));
      int pixel_y = int(viewport_size.y * (sspos.y * 0.5 + 0.5));
      sspos.x = 2.0 * (float(pixel_x)) / viewport_size.x - 1.0;
      sspos.y = 2.0 * (float(pixel_y)) / viewport_size.y - 1.0;
      sspos.z = instance_offset.z;
      gl_Position = sspos;

  })";
      const GLchar *fsrc =
          R"(#version 300 es
          precision highp float;
  layout(location = 0) out vec4 SV_TARGET0;

  in vec2 uv;
  in vec3 color;

  uniform sampler2D image;
  void main() {
    if (texture(image, uv).x > 0.0)
      SV_TARGET0 = vec4(color, 1.0);
    else
      discard;
  })";
      static GLuint program      = create_program(vsrc, fsrc);
      static GLuint font_texture = [&] {
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        uint8_t *r8_data = (uint8_t *)malloc(simplefont_bitmap_height * simplefont_bitmap_width);
        defer(free(r8_data));
        ito(simplefont_bitmap_height) {
          jto(simplefont_bitmap_width) {
            char c                                   = simplefont_bitmap[i][j];
            r8_data[(i)*simplefont_bitmap_width + j] = c == ' ' ? 0 : 0xff;
          }
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, simplefont_bitmap_width, simplefont_bitmap_height, 0,
                     GL_RED, GL_UNSIGNED_BYTE, r8_data);
        return tex;
      }();
      static GLuint vao;
      static GLuint vbo;
      static GLuint instance_vbo;
      static int    glyph_vao = [&] {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        float pos[] = {
            0.0f, 0.0f, //
            1.0f, 0.0f, //
            1.0f, 1.0f, //
            0.0f, 0.0f, //
            1.0f, 1.0f, //
            0.0f, 1.0f, //
        };
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_DYNAMIC_DRAW);
        glGenBuffers(1, &instance_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, instance_vbo);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        return 0;
      }();
      (void)glyph_vao;
      struct Glyph_Instance_GL {
        float x, y, z;
        float u, v;
        float r, g, b;
      };
      static_assert(sizeof(Glyph_Instance_GL) == 32, "");
      float    glyph_uv_width  = (float)simplefont_bitmap_glyphs_width / simplefont_bitmap_width;
      float    glyph_uv_height = (float)simplefont_bitmap_glyphs_height / simplefont_bitmap_height;

      float      glyph_pad_ss   = 2.0f / viewport_width;
      uint32_t   max_num_glyphs = 0;
      uint32_t   num_strings    = string_storage.cursor;
      _String2D *strings        = string_storage.at(0);
      kto(num_strings) { max_num_glyphs += (uint32_t)strings[k].len; }
      TMP_STORAGE_SCOPE;
      Glyph_Instance_GL *glyphs_gl =
          (Glyph_Instance_GL *)tl_alloc_tmp(sizeof(Glyph_Instance_GL) * max_num_glyphs);
      uint32_t num_glyphs = 0;

      kto(num_strings) {
        _String2D string = strings[k];
        if (string.len == 0) continue;
        float2 ss = (float2){string.x, string.y};
        if (string.world_space) {
          ss = camera.world_to_screen(ss);
        } else {
          ss = camera.window_to_screen((int2){(int32_t)ss.x, (int32_t)ss.y});
        }
        float min_ss_x = ss.x;
        float min_ss_y = ss.y;
        float max_ss_x = ss.x + (camera.glyphs_screen_width + glyph_pad_ss) * string.len;
        float max_ss_y = ss.y + camera.glyphs_screen_height;
        if (min_ss_x > 1.0f || min_ss_y > 1.0f || max_ss_x < -1.0f || max_ss_y < -1.0f) continue;

        ito(string.len) {
          uint32_t c = (uint32_t)string.c_str[i];

          // Printable characters only
          c            = clamp(c, 0x20u, 0x7eu);
          uint32_t row = (c - 0x20) / simplefont_bitmap_glyphs_per_row;
          uint32_t col = (c - 0x20) % simplefont_bitmap_glyphs_per_row;
          float    v0 =
              ((float)row * (simplefont_bitmap_glyphs_height + simplefont_bitmap_glyphs_pad_y * 2) +
               simplefont_bitmap_glyphs_pad_y) /
              simplefont_bitmap_height;
          float u0 =
              ((float)col * (simplefont_bitmap_glyphs_width + simplefont_bitmap_glyphs_pad_x * 2) +
               simplefont_bitmap_glyphs_pad_x) /
              simplefont_bitmap_width;
          Glyph_Instance_GL glyph;
          glyph.u                 = u0;
          glyph.v                 = v0;
          glyph.x                 = ss.x + (camera.glyphs_screen_width + glyph_pad_ss) * i;
          glyph.y                 = ss.y;
          glyph.z                 = string.z;
          glyph.r                 = string.color.r;
          glyph.g                 = string.color.g;
          glyph.b                 = string.color.b;
          glyphs_gl[num_glyphs++] = glyph;
        }
      }
      if (num_glyphs == 0) goto skip_strings;
      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glEnableVertexAttribArray(0);
      // glVertexAttribBinding(0, 0);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
      glBindBuffer(GL_ARRAY_BUFFER, instance_vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(Glyph_Instance_GL) * num_glyphs, glyphs_gl,
                   GL_DYNAMIC_DRAW);
      glEnableVertexAttribArray(1);
      // glVertexAttribBinding(1, 0);
      glVertexAttribDivisor(1, 1);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 32, (void *)0);
      glEnableVertexAttribArray(2);
      // glVertexAttribBinding(2, 0);
      glVertexAttribDivisor(2, 1);
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 32, (void *)12);
      glEnableVertexAttribArray(3);
      // glVertexAttribBinding(3, 0);
      glVertexAttribDivisor(3, 1);
      glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 32, (void *)20);
      // Draw

      glUseProgram(program);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, font_texture);
      glUniform1i(glGetUniformLocation(program, "image"), 0);
      //      glUniformMatrix4fv(glGetUniformLocation(line_program,
      //      "projection"), 1,
      //                         GL_FALSE, (float *)&camera.proj[0]);
      glUniform2f(glGetUniformLocation(program, "viewport_size"), (float)viewport_width,
                  (float)viewport_height);
      glUniform2f(glGetUniformLocation(program, "glyph_size"), camera.glyphs_screen_width,
                  camera.glyphs_screen_height);
      glUniform2f(glGetUniformLocation(program, "glyph_uv_size"), glyph_uv_width, glyph_uv_height);

      glDrawArraysInstanced(GL_TRIANGLES, 0, 6, num_glyphs);
      glBindVertexArray(0);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
    }
  skip_strings:
    (void)0;
  }
}

// void compile_shader(GLuint shader) {
//  glCompileShader(shader);
//  GLint isCompiled = 0;
//  glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
//  if (isCompiled == GL_FALSE) {
//    GLint maxLength = 0;
//    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

//    // The maxLength includes the NULL character
//    std::vector<GLchar> errorLog(maxLength);
//    glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);

//    glDeleteShader(shader);
//    fprintf(stderr, "[ERROR]: %s\n", &errorLog[0]);
//    exit(1);
//  }
//}

// void link_program(GLuint program) {
//  glLinkProgram(program);
//  GLint isLinked = 0;
//  glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
//  if (isLinked == GL_FALSE) {
//    GLint maxLength = 0;
//    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

//    // The maxLength includes the NULL character
//    std::vector<GLchar> infoLog(maxLength);
//    glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

//    // The program is useless now. So delete it.
//    glDeleteProgram(program);
//    fprintf(stderr, "[ERROR]: %s\n", &infoLog[0]);
//    exit(1);
//  }
//}

GLuint initShader() {
  const GLchar *vertexSource =
      R"(#version 300 es
  precision highp float;
  layout (location=0) in vec3 position;
  out vec3 color;
  void main() {
      color = position;
      gl_Position = vec4(position, 1.0);

  })";

  // Fragment/pixel shader
  const GLchar *fragmentSource =
      R"(#version 300 es
  precision highp float;
  in vec3 color;
  layout(location = 0) out vec4 SV_TARGET0;

  void main() {
    SV_TARGET0 = vec4(color, 1.0);
  })";
  // Create and compile vertex shader
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexSource, NULL);
  compile_shader(vertexShader);

  // Create and compile fragment shader
  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
  compile_shader(fragmentShader);

  // Link vertex and fragment shader into shader program and use it
  GLuint program = glCreateProgram();
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  link_program(program);
  glDetachShader(program, vertexShader);
  glDetachShader(program, fragmentShader);
  glUseProgram(program);
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
  return program;
}

GLuint initGeometry(GLuint program) {
  // Create vertex buffer object and copy vertex data into it
  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  GLfloat vertices[] = {0.0f, 0.5f, 0.0f, -0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0.0f};
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  // Specify the layout of the shader vertex data (positions only, 3 floats)
  GLint posAttrib = glGetAttribLocation(program, "position");
  glEnableVertexAttribArray(posAttrib);
  glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 0, 0);
  glBindVertexArray(0);
  return vao;
}

void redraw() {
  GLuint program = initShader();
  GLuint vao     = initGeometry(program);
  glUseProgram(program);
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glDeleteProgram(program);
  glDeleteVertexArrays(1, &vao);
}

// struct Console {
//    char     buffer[0x100][0x100];
//    uint32_t column    = 0;
//    uint32_t scroll_id = 0;
//    Console() { memset(this, 0, sizeof(*this)); }
//    void unscroll() {
//      if (scroll_id != 0) {
//        memcpy(&buffer[0][0], &buffer[scroll_id][0], 0x100);
//        scroll_id = 0;
//      }
//    }
//    void backspace() {
//      unscroll();
//      if (column > 0) {
//        ito(0x100 - column) { buffer[0][column + i - 1] = buffer[0][column + i]; }
//        column--;
//      }
//    }
//    void newline() {
//      unscroll();
//      ito(0x100 - 1) { memcpy(&buffer[0x100 - 1 - i][0], &buffer[0x100 - 2 - i][0], 0x100); }
//      memset(&buffer[0][0], 0, 0x100);
//      column = 0;
//    }
//    void cursor_right() {
//      unscroll();
//      if (buffer[0][column] != '\0') column++;
//    }
//    void cursor_left() {
//      unscroll();
//      if (column > 0) column--;
//    }
//    void put_line(char const *str) {
//      unscroll();
//      while (str[0] != '\0') {
//        put_char(str[0]);
//        str++;
//      }
//      newline();
//    }
//    void put_fmt(char const *fmt, ...) {
//      char    buffer[0x100];
//      va_list args;
//      va_start(args, fmt);
//      vsnprintf(buffer, sizeof(buffer), fmt, args);
//      va_end(args);
//      put_line(buffer);
//    }
//    void put_char(char c) {
//      unscroll();
//      if (c >= 0x20 && c <= 0x7e && column < 0x100 - 1) {
//        ito(0x100 - column - 1) { buffer[0][0x100 - i - 1] = buffer[0][0x100 - i - 2]; }
//        buffer[0][column++] = c;
//      }
//    }
//    void scroll_up() {
//      if (scroll_id < 0x100) {
//        scroll_id++;
//        column = strlen(buffer[scroll_id]);
//      }
//    }
//    void scroll_down() {
//      if (scroll_id > 0) {
//        scroll_id--;
//        column = strlen(buffer[scroll_id]);
//      }
//    }
//  } console;
//  bool console_mode = false;
//  void draw_console() {
//    float    CONSOLE_TEXT_LAYER       = 102.0f / 256.0f;
//    float    CONSOLE_CURSOR_LAYER     = 101.0f / 256.0f;
//    float    CONSOLE_BACKGROUND_LAYER = 100.0f / 256.0f;
//    float    GLYPH_HEIGHT             = camera.glyph_scale * simplefont_bitmap_glyphs_height;
//    float    GLYPH_WIDTH              = camera.glyph_scale * simplefont_bitmap_glyphs_width;
//    uint32_t console_lines            = 8;
//    float    console_bottom           = (GLYPH_HEIGHT + 1) * console_lines;
//    ito(console_lines - 1) {
//      draw_string({.c_str       = console.buffer[console_lines - 1 - i],
//                   .x           = 0,
//                   .y           = (GLYPH_HEIGHT + 1) * (i + 1),
//                   .z           = CONSOLE_TEXT_LAYER,
//                   .color       = {.r = 0.0f, .g = 0.0f, .b = 0.0f},
//                   .world_space = false});
//    }
//    draw_string({.c_str       = console.buffer[console.scroll_id],
//                 .x           = 0.0f,
//                 .y           = console_bottom,
//                 .z           = CONSOLE_TEXT_LAYER,
//                 .color       = {.r = 0.0f, .g = 0.0f, .b = 0.0f},
//                 .world_space = false});
//    draw_rect({//
//               .x           = 0.0f,
//               .y           = 0.0f,
//               .z           = CONSOLE_BACKGROUND_LAYER,
//               .width       = (float)camera.viewport_width,
//               .height      = console_bottom + 1.0f,
//               .color       = {.r = 0.8f, .g = 0.8f, .b = 0.8f},
//               .world_space = false});
//    draw_line({//
//               .x0          = 0.0f,
//               .y0          = console_bottom + 2.0f,
//               .x1          = (float)camera.viewport_width,
//               .y1          = console_bottom + 2.0f,
//               .z           = CONSOLE_CURSOR_LAYER,
//               .color       = {.r = 0.0f, .g = 0.0f, .b = 0.0f},
//               .world_space = false});
//    draw_rect({//
//               .x           = console.column * (GLYPH_WIDTH + 1.0f),
//               .y           = console_bottom,
//               .z           = CONSOLE_CURSOR_LAYER,
//               .width       = GLYPH_WIDTH,
//               .height      = -GLYPH_HEIGHT,
//               .color       = {.r = 1.0f, .g = 1.0f, .b = 1.0f},
//               .world_space = false});
//  }
