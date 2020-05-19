#ifndef NODES_HPP
#define NODES_HPP

#include "node_editor.h"

enum class Node_t {
  UNKNOWN = 0,
  GFX_DRAW_CALL,
  GFX_PASS,
};

static char const *Node_Type_Name_Table[]{
    "Gfx/DrawCall",
    "Gfx/Pass",
    NULL,
};

static Node_t Node_Type_Table[]{
    Node_t::GFX_DRAW_CALL,
    Node_t::GFX_PASS,
    Node_t::UNKNOWN,
};

static Node_t str_to_node_type(string_ref str) {
  ito(ARRAY_SIZE(Node_Type_Name_Table)) {
    if (Node_Type_Name_Table[i] == NULL) return Node_t::UNKNOWN;
    if (str == stref_s(Node_Type_Name_Table[i])) {
      return Node_Type_Table[i];
    }
  }
  return Node_t::UNKNOWN;
}

static char const *node_type_to_str(Node_t type) {
  ito(ARRAY_SIZE(Node_Type_Table)) {
    if (Node_Type_Table[i] == Node_t::UNKNOWN) return "UNKNOWN";
    if (type == Node_Type_Table[i]) {
      return Node_Type_Name_Table[i];
    }
  }
  return "UNKNOWN";
}

struct Node {
  //
  // +-------+ ^
  // |       | |
  // |   +   | | size.y * 2
  // |   pos | |
  // +-------+ V
  // <------->
  //   size.x * 2
  //
  u32    id;
  char   name[0x10];
  Node_t type;
  float2 pos;
  float2 size;
  // flags
  bool selected;
  bool hovered;
  bool dragged;
  bool inside(float x, float y) {
    return x > pos.x - size.x && //
           x < pos.x + size.x && //
           y > pos.y - size.y && //
           y < pos.y + size.y;
  }
};

#endif // NODES_HPP
