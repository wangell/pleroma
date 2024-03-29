#include "net.h"

#include "../hylic_ast.h"
#include "../hylic_eval.h"
#include "../type_util.h"
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

// Host -> Entity
std::map<std::string, std::tuple<EntityRefNode *, std::string>> host_entity_lookup;

AstNode *net_return_http_result(EvalContext *context, std::vector<AstNode *> args) {
  assert(args[0]->type == AstNodeType::StringNode);

  auto res_str = (StringNode *)args[0];
  printf("calling return result %s\n", res_str->value.c_str());
  send(new_socket, res_str->value.c_str(), strlen(res_str->value.c_str()), 0);
  close(new_socket);

  eval_message_node(context, (EntityRefNode *)make_entity_ref(-1, -1, -1), CommMode::Async, "next", {});

  return make_nop();
}

AstNode *net_start(EvalContext *context, std::vector<AstNode *> args) {

  // Creating socket file descriptor
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // Forcefully attaching socket to the port 8080
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
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

  std::string hostname = ((StringNode *)args[0])->value;
  auto entity_ref = ((EntityRefNode *)args[1]);
  std::string callback = ((StringNode *)args[2])->value;

  printf("registered %s\n", hostname.c_str());
  host_entity_lookup[hostname] =
      std::make_tuple((EntityRefNode *)make_entity_ref(entity_ref->node_id, entity_ref->vat_id, entity_ref->entity_id), callback);

  return make_number(0);
}

AstNode *net_next(EvalContext *context, std::vector<AstNode *> args) {
  if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen), SOCK_NONBLOCK) < 0) {
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

  std::string header, header_contents;
  std::string hostname;

  while (iss >> header) {
    iss >> header_contents;

    if (header == "Host:") {
      hostname = header_contents;
    }
  }

  if (host_entity_lookup.find(hostname) == host_entity_lookup.end()) {
    printf("couldn't find it\n");
    std::string rsp404 = "HTTP/1.1 404 Not Found";
    send(new_socket, rsp404.c_str(), strlen(rsp404.c_str()), 0);
    close(new_socket);
    return make_number(0);
  }
  auto host_ref = host_entity_lookup[hostname];
  // AstNode* res = eval_message_node(context, (EntityRefNode*)make_entity_ref(0, 0, 2), MessageDistance::Local, CommMode::Sync, "test",
  // {make_string(buffer)});
  PromiseNode *res = (PromiseNode *)eval_message_node(context, std::get<0>(host_ref), CommMode::Async, std::get<1>(host_ref), {make_string(verb), make_string(path)});

  eval_message_node(context, make_self(), CommMode::Async, "return-http-result", {res});
  //{make_message_node(make_self(), "return-http-result", CommMode::Async, {res})}));
  // context->vat->promises[res->promise_id].callbacks.push_back((PromiseResNode *)make_promise_resolution_node("anon",
  // {make_message_node(make_self(), "return-http-result", CommMode::Async, {res})}));

  //context->vat->promises[res->promise_id].callbacks.push_back((PromiseResNode *)make_promise_resolution_node(
  //    "anon", {make_message_node(make_entity_ref(0, 0, 1), "print", CommMode::Async, {make_string("blah")})}));

  return make_number(0);
}

AstNode *net_create(EvalContext *context, std::vector<AstNode *> args) { return make_number(0); }

std::map<std::string, AstNode *> load_net() {
  std::map<std::string, FuncStmt *> functions;

  CType test_type;
  test_type.basetype = PType::u8;

  CType none_type;
  none_type.basetype = PType::None;
  none_type.dtype = DType::Local;

  CType *blah = new CType;
  blah->basetype = PType::BaseEntity;

  CType *blarg = new CType;
  blarg->basetype = PType::str;

  functions["start"] = setup_direct_call(net_start, "start", {"host", "e", "func"}, {blarg, blah, blarg}, none_type);
  functions["next"] = setup_direct_call(net_next, "next", {}, {}, none_type);
  functions["create"] = setup_direct_call(net_create, "create", {}, {}, none_type);
  functions["return-http-result"] = setup_direct_call(net_return_http_result, "return-http-result", {"res"}, {blarg}, none_type);

  kernel_map["HttpLb"] = make_actor(nullptr, "HttpLb", functions, {}, {}, {}, {});

  return {{"HttpLb", make_actor(nullptr, "HttpLb", functions, {}, {}, {}, {})}};
}
