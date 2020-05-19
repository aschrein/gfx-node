#include "node_editor.h"
#include "nodes.hpp"
#include <json.hpp>

void Scene::init_from_json(char const *str) {
  using json  = nlohmann::json;
  json parsed = json::parse(str);
  ASSERT_DEBUG(parsed.is_object());
  u32  last_node_id = parsed["last_node_id"].get<u32>();
  u32  last_link_id = parsed["last_link_id"].get<u32>();
  auto nodes        = parsed.at("nodes");
  auto links        = parsed.at("links");
  auto payload      = parsed.at("payload");
  auto camera       = parsed.at("camera");
  ASSERT_DEBUG(nodes.is_array());
  ASSERT_DEBUG(links.is_array());
  ASSERT_DEBUG(payload.is_object());
  ASSERT_DEBUG(camera.is_object());
  for (auto &item : nodes) {
    add_node(item.at("name").get<std::string>().c_str(), //
             item.at("type").get<std::string>().c_str(), //
             item.at("x").get<float>(),                  //
             item.at("y").get<float>(),                  //
             item.at("size_x").get<float>(),             //
             item.at("size_y").get<float>()              //
    );
  }
  c2d.camera.pos.x = camera.at("x").get<float>();
  c2d.camera.pos.y = camera.at("y").get<float>();
  c2d.camera.pos.z = camera.at("z").get<float>();
  auto sources     = payload.at("srcs");
  ASSERT_DEBUG(sources.is_array());
  for (auto &item : sources) {
    add_source(item.at(0).get<std::string>().c_str(), item.at(1).get<std::string>().c_str());
  }
}

string_ref Scene::to_json_tmp() {
  using json = nlohmann::json;
  json root;
  root["last_node_id"]    = 0;
  root["last_link_id"]    = 0;
  root["nodes"]           = json::array();
  root["links"]           = json::array();
  root["payload"]         = json::object();
  root["camera"]          = json::object();
  root["camera"]["x"]     = c2d.camera.pos.x;
  root["camera"]["y"]     = c2d.camera.pos.y;
  root["camera"]["z"]     = c2d.camera.pos.z;
  root["payload"]["srcs"] = json::array();
  {
    char const **src_names = NULL;
    u32          src_count = 0;
    get_source_list(&src_names, &src_count);
    ito(src_count) {
      auto item = json::array();
      item.push_back(std::string(src_names[i]));
      item.push_back(std::string(get_source(src_names[i])));
      root["payload"]["srcs"].push_back(item);
    }
  }
  {
    Node *nodes      = NULL;
    u32   node_count = 0;
    get_nodes(&nodes, &node_count);
    ito(node_count) {
      Node &node          = nodes[i];
      auto  node_json     = json::object();
      node_json["x"]      = node.pos.x;
      node_json["y"]      = node.pos.y;
      node_json["size_x"] = node.size.y;
      node_json["size_y"] = node.size.x;
      node_json["name"]   = std::string(node.name);
      node_json["type"]   = std::string(node_type_to_str(node.type));
      root["nodes"].push_back(node_json);
    }
  }
  std::string str = root.dump();
  char *      ptr = (char *)tl_alloc_tmp(str.size());
  memcpy(ptr, str.c_str(), str.size());
  return string_ref{.ptr = ptr, .len = str.size()};
}
