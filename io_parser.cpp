#include <json.hpp>

#include "node_editor.h"

void parse_json(char const *str) {
  using json = nlohmann::json;
  json parsed = json::parse(str);

}
