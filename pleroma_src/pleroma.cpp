#include "pleroma.h"
#include "general_util.h"
#include "hylic.h"
#include "hylic_ast.h"
#include "hylic_eval.h"
#include <locale>
#include <map>
#include <queue>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <vector>

#include <thread>

#include "hylic_typesolver.h"
#include "netcode.h"
#include "core/kernel.h"
#include "blockingconcurrentqueue.h"
#include "node_config.h"

#include "other.h"
#include "system.h"

//const auto processor_count = std::thread::hardware_concurrency();
const auto processor_count = 1;
const int MAX_STEPS = 3;

PleromaNode *this_pleroma_node;

moodycamel::BlockingConcurrentQueue<Vat*> queue;

Msg create_response(Msg msg_in, AstNode *return_val) {
  Msg response_m;
  response_m.response = true;

  response_m.entity_id = msg_in.src_entity_id;
  response_m.vat_id = msg_in.src_vat_id;
  response_m.node_id = msg_in.src_node_id;

  response_m.src_entity_id = msg_in.entity_id;
  response_m.src_vat_id = msg_in.vat_id;
  response_m.src_node_id = msg_in.node_id;
  response_m.promise_id = msg_in.promise_id;

  if (return_val->type == AstNodeType::NumberNode || return_val->type == AstNodeType::StringNode || return_val->type == AstNodeType::EntityRefNode) {
    response_m.values.push_back((ValueNode*)return_val);
  }

  return response_m;
}

void process_vq() {
  while (true) {
    Vat* our_vat;
    queue.wait_dequeue(our_vat);

    // Take a number of steps
    for (int k = 0; k < 1; ++k) {

      while (!our_vat->messages.empty()) {
        Msg m = our_vat->messages.front();
        our_vat->messages.pop();
        print_msg(&m);

        try {
          auto find_entity = our_vat->entities.find(m.entity_id);
          assert(find_entity != our_vat->entities.end());
          Entity* target_entity = find_entity->second;

          EvalContext context;
          start_context(&context, this_pleroma_node, our_vat, target_entity->entity_def->module, target_entity);

          // Return vs call
          if (m.response) {
            // If we didn't setup a promise to resolve, then ignore the result
            if (our_vat->promises.find(m.promise_id) != our_vat->promises.end() && our_vat->promises[m.promise_id].callback) {
              our_vat->promises[m.promise_id].results = m.values;
              eval_promise_local(&context, our_vat->entities.find(m.entity_id)->second, &our_vat->promises[m.promise_id]);
            }
          } else {
            std::vector<AstNode *> args;

            for (int zz = 0; zz < m.values.size(); ++zz) {
              args.push_back(m.values[zz]);
            }

            auto result = eval_func_local(&context, target_entity, m.function_name, args);

            // All return values are singular - we use tuples to represent
            // multiple return values
            // FIXME might not work if we handle tuples differently
            Msg response_m = create_response(m, result);

            // Main cannot be called by any function except ours, move this logic into typechecker
            if (m.function_name != "main") {
              our_vat->out_messages.push(response_m);
            }
          }
        } catch (PleromaException &e) {
          printf("PleromaException: %s\n", e.what());
          printf("Calling message: \n");
          print_msg(&m);
          throw;
        }
      }

      while (!our_vat->out_messages.empty()) {
        Msg m = our_vat->out_messages.front();
        our_vat->out_messages.pop();
        //print_msg(&m);

        // If we're communicating on the same node, we don't have to use the router
        if (m.node_id == this_pleroma_node->node_id && m.vat_id == our_vat->id) {
          our_vat->messages.push(m);
        } else {
          net_out_queue.enqueue(m);
        }
      }

      //sleep(1);

      our_vat->run_n++;
    }

    net_vats.enqueue(our_vat);

  }
}

void start_program(std::string program_name, std::string ent_name) {
  Msg m;
  m.node_id = monad_ref->node_id;
  m.vat_id = monad_ref->vat_id;
  m.entity_id = monad_ref->entity_id;
  m.function_name = "start-program";

  m.values.push_back((StringNode*)make_string(program_name));
  m.values.push_back((StringNode *)make_string(ent_name));

  m.src_entity_id = -1;
  m.src_node_id = -1;
  m.src_vat_id = -1;
  m.promise_id = -1;

  net_out_queue.enqueue(m);
}

void inoculate_pleroma(HylicModule *ukernel, std::string ent0) {

  EntityDef *ent0_def = (EntityDef *)ukernel->entity_defs[ent0];

  Vat* og_vat = new Vat;
  og_vat->id = 0;
  queue.enqueue(og_vat);
  this_pleroma_node->vat_id_base++;

  EvalContext context;
  start_context(&context, this_pleroma_node, og_vat, ukernel, nullptr);

  Entity *ent = create_entity(&context, ent0_def, false);
  ent->module_scope = ukernel;

  monad_ref = (EntityRefNode*)make_entity_ref(ent->address.node_id, ent->address.vat_id, ent->address.entity_id);
  //printf("%d %d %d\n", monad_ref->node_id, monad_ref->vat_id, monad_ref->entity_id);

  og_vat->entities[0] = ent;

  Msg m;
  m.entity_id = 0;
  m.function_name = "hello";
  m.node_id = 0;

  m.src_entity_id = -1;
  m.src_node_id = -1;
  m.src_vat_id = -1;
  m.promise_id = -1;

  m.values.push_back((ValueNode *)make_number(0));

  og_vat->messages.push(m);
}

EntityAddress start_nodeman(HylicModule *ukernel, std::string ent0) {

  EntityDef *ent0_def = (EntityDef *)ukernel->entity_defs[ent0];

  Vat *og_vat = new Vat;
  og_vat->id = this_pleroma_node->vat_id_base;
  queue.enqueue(og_vat);
  this_pleroma_node->vat_id_base++;

  EvalContext context;
  start_context(&context, this_pleroma_node, og_vat, ukernel, nullptr);

  Entity *ent = create_entity(&context, ent0_def, false);
  ent->module_scope = ukernel;

  og_vat->entities[0] = ent;

  return ent->address;
}

void usage() {
  printf("Usage: ...\n");
}

struct ConnectionInfo {
  std::string host_ip;
  int host_port;

  std::string first_contact_ip;
  int first_contact_port;
};

void start_pleroma(ConnectionInfo connect_info) {
  dbp(log_debug, "Reading node config [pleroma.json]...");

  this_pleroma_node = read_node_config();
  add_new_pnode(this_pleroma_node);

  load_kernel();

  if (connect_info.first_contact_ip == "") {
    dbp(log_debug, "Inoculating Pleroma [Monad]...");
    auto monad = load_system_module(SystemModule::Monad);
    inoculate_pleroma(monad, "Monad");
    dbp(log_debug, "Successfully inoculated.");
  }

  dbp(log_debug, "Initializing host [%s : %d]...", connect_info.host_ip.c_str(), connect_info.host_port);
  init_network();
  setup_server(connect_info.host_ip, connect_info.host_port);
  dbp(log_debug, "Host initialized");

  auto ent_add = start_nodeman(load_system_module(SystemModule::Monad), "NodeMan");
  this_pleroma_node->nodeman_addr = ent_add;

  if (connect_info.first_contact_ip != "") {
    dbp(log_info, "Connecting to network [%s : %d]...", connect_info.first_contact_ip.c_str(), connect_info.first_contact_port);
    connect_to_cluster(mk_netaddr(connect_info.first_contact_ip, connect_info.first_contact_port));
    dbp(log_info, "Successfully connected");

    start_program("helloworld", "UserProgram");
  }

  load_software();

  std::thread burners[processor_count];

  dbp(log_debug, "Starting %d burner processes...", processor_count);
  for (int k = 0; k < processor_count; ++k) {
    burners[k] = std::thread(process_vq);
  }

  dbp(log_debug, "Starting net loop...");
  while (true) {
    net_loop();
  }

  dbp(log_debug, "Net loop finished, joining all processes");
  for (int k = 0; k < processor_count; ++k) {
    burners[k].join();
  }

  dbp(log_info, "Burners joined, exiting.");
}

int main(int argc, char **argv) {
  setlocale(LC_ALL, "");

  if (argc < 2) {
    usage();
    exit(1);
  }

  if (std::string(argv[1]) == "start") {

    ConnectionInfo connect_info;
    if (argc < 4) {
      connect_info.host_ip = "0.0.0.0";
      connect_info.host_port = 8080;
    } else {
      connect_info.host_ip = argv[2];
      connect_info.host_port = std::stoi(argv[3]);
    }

    //throw PleromaException(
    //    "Usage: ./pleroma start hostIP hostPort [clientIP clientPort].");
    if (argc > 4) {
      connect_info.first_contact_ip = argv[4];
      connect_info.first_contact_port = std::stoi(argv[5]);
    }

    start_pleroma(connect_info);
  } else if (std::string(argv[1]) == "test") {
    std::string target_file = argv[2];
    load_file("test", target_file);
    exit(0);
  } else {
    usage();
    exit(1);
  }

}
