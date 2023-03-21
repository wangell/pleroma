#include "node_config.h"
#include "../other_src/json.hpp"
#include "general_util.h"
#include "hylic_eval.h"
#include "other.h"
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

  pnode->node_name = json_config["name"];

  if (pnode->node_name == "") {
    throw PleromaException("Node name must be configured.");
  }

  for (auto &k : json_config["resources"]) {
    pnode->resources.push_back(k);
  }

  std::string debug_str = "Node configured (" + config_path + "):\n";
  debug_str += "\tNode name: " + pnode->node_name + "\n";
  debug_str += "\tResources:\n";

  for (auto &k : pnode->resources) {
    debug_str += "\t\t- " + k + "\n";
  }

  dbp(log_debug, debug_str.c_str());

  return pnode;
}
