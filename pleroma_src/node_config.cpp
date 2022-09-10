#include "node_config.h"
#include "../other_src/json.hpp"
#include "hylic_eval.h"
#include <fstream>
#include <sstream>
#include <string>

using json = nlohmann::json;

PleromaNode *read_node_config(std::string config_path) {
  PleromaNode* pnode = new PleromaNode;

  std::string config_file_contents;
  std::ifstream config_file;

  config_file.open(config_path);

  std::stringstream filestream;
  filestream << config_file.rdbuf();
  config_file.close();
  config_file_contents = filestream.str();

  json json_config = json::parse(config_file_contents);

  std::string test_string = json_config["name"];

  for (auto &k : json_config["resources"]) {
    pnode->resources.push_back(k);
  }

  return pnode;
}
