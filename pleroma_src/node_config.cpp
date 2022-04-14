#include "node_config.h"
#include "../other_src/json.hpp"
#include <fstream>
#include <sstream>
#include <string>

using json = nlohmann::json;

void read_node_config() {
  std::string config_file_contents;
  std::ifstream config_file;

  config_file.open("pleroma.json");

  std::stringstream filestream;
  filestream << config_file.rdbuf();
  config_file.close();
  config_file_contents = filestream.str();

  json json_config = json::parse(config_file_contents);

  std::string test_string = json_config["name"];
  printf("Test %s\n", test_string.c_str());
}
