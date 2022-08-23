#include "net.h"

#include "../hylic_ast.h"
#include "../hylic_eval.h"
#include "ffi.h"

#include <cstring>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

extern std::map<std::string, AstNode *> kernel_map;

int server_fd, new_socket, valread;
struct sockaddr_in address;
int opt = 1;
int addrlen = sizeof(address);
char buffer[1024] = {0};
char *hello = "Hello from server";

AstNode* entity_ref;

AstNode *net_start(EvalContext *context, std::vector<AstNode *> args) {

  // Creating socket file descriptor
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // Forcefully attaching socket to the port 8080
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(8080);

  // Forcefully attaching socket to the port 8080
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }
  if (listen(server_fd, 3) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  entity_ref = eval(context, args[0]);

  //printf("%d Entity ref %d %d %d\n", eref->type, eref->node_id, eref->vat_id, eref->entity_id);

  return make_number(0);
}

AstNode *net_next(EvalContext *context, std::vector<AstNode *> args) {
  if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                           (socklen_t *)&addrlen)) < 0) {
    perror("accept");
    exit(EXIT_FAILURE);
  }
  valread = read(new_socket, buffer, 1024);

  std::string verb;
  std::string path;
  std::string version;

  std::istringstream iss(buffer, std::istringstream::in);

  iss >> verb;
  iss >> path;
  iss >> version;

  // Call our function here
  //AstNode* res = eval_message_node(context, (EntityRefNode*)make_entity_ref(0, 0, 2), MessageDistance::Local, CommMode::Sync, "test", {make_string(buffer)});
  AstNode *res = eval_message_node(context, (EntityRefNode*)entity_ref, CommMode::Sync, "test", {make_string(verb), make_string(path)});

  auto res_str = (StringNode*) eval(context, res);

  //printf("Response %s\n", res_str->value.c_str());
  send(new_socket, res_str->value.c_str(), strlen(res_str->value.c_str()), 0);
  close(new_socket);

  return make_number(0);
}

AstNode *net_create(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

void load_net() {
  std::map<std::string, FuncStmt *> functions;

  CType test_type;
  test_type.basetype = PType::u8;

  CType blah;
  blah.basetype = PType::Entity;

  functions["start"] = setup_direct_call(net_start, "start", {"ent"}, {&blah}, test_type);
  functions["next"] = setup_direct_call(net_next, "next", {}, {}, test_type);
  functions["create"] = setup_direct_call(net_create, "create", {}, {}, test_type);
  functions["create"]->ctype.basetype = PType::u8;

  kernel_map["Net"] = make_actor(nullptr, "Net", functions, {}, {});
}
