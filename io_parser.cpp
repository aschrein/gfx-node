#include <json.hpp>

#include "node_editor.h"

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
  char const **src_names  = NULL;
  u32          src_count  = 0;
  get_source_list(&src_names, &src_count);
  ito(src_count) {
    auto item = json::array();
    item.push_back(std::string(src_names[i]));
    item.push_back(std::string(get_source(src_names[i])));
    root["payload"]["srcs"].push_back(item);
  }
  std::string str = root.dump();
  char *      ptr = (char *)tl_alloc_tmp(str.size());
  memcpy(ptr, str.c_str(), str.size());
  return string_ref{.ptr = ptr, .len = str.size()};
}
