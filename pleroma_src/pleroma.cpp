#include "pleroma.h"
#include "hylic.h"
#include "hylic_ast.h"
#include "hylic_eval.h"
#include <locale>
#include <map>
#include <queue>
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

// const auto processor_count = std::thread::hardware_concurrency();
const auto processor_count = 1;
const int MAX_STEPS = 3;

PleromaNode this_pleroma_node;

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

  if (return_val->type == AstNodeType::NumberNode || return_val->type == AstNodeType::StringNode) {
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

        auto find_entity = our_vat->entities.find(m.entity_id);
        assert(find_entity != our_vat->entities.end());
        Entity* target_entity = find_entity->second;

        EvalContext context;
        Scope scope;
        start_stack(&context, &scope, our_vat, target_entity);
        context.node = &this_pleroma_node;

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
      }

      while (!our_vat->out_messages.empty()) {
        Msg m = our_vat->out_messages.front();
        our_vat->out_messages.pop();

        // If we're communicating on the same node, we don't have to use the router
        if (m.node_id == this_pleroma_node.node_id && m.vat_id == our_vat->id) {
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

void inoculate_pleroma(HylicModule *program, EntityDef *entity_def) {
  Vat* og_vat = new Vat;
  og_vat->id = 0;
  queue.enqueue(og_vat);
  this_pleroma_node.vat_id_base++;

  EvalContext context;
  context.entity = nullptr;
  context.vat = og_vat;
  context.node = &this_pleroma_node;

  Scope scope;
  scope.table = program->entity_defs;
  context.scope = &scope;

  Entity *ent = create_entity(&context, entity_def, false);
  ent->module_scope = program;

  og_vat->entities[0] = ent;

  Msg m;
  m.entity_id = 0;
  m.function_name = "main";
  m.node_id = 0;

  m.src_entity_id = -1;
  m.src_node_id = -1;
  m.src_vat_id = -1;
  m.promise_id = -1;

  m.values.push_back((ValueNode *)make_number(0));

  og_vat->messages.push(m);
}

int main(int argc, char **argv) {

  setlocale(LC_ALL, "");

  read_node_config();

  auto program = load_file("examples/kernel.po");

  load_kernel();

  auto user_program = (EntityDef *)program.entity_defs["Test"];

  program.entity_defs["Io"] = (EntityDef *)kernel_map["Io"];
  program.entity_defs["Net"] = (EntityDef *)kernel_map["Net"];
  //program["Amoeba"] = (EntityDef *)kernel_map["Amoeba"];

  //typesolve(program);

  inoculate_pleroma(&program, user_program);

  init_network();

  setup_server(argv[1], std::stoi(argv[2]));

  if (argc > 3) {
    connect_client(std::string(argv[3]), std::stoi(argv[4]));
  }

  std::thread burners[processor_count];

  for (int k = 0; k < processor_count; ++k) {
    burners[k] = std::thread(process_vq);
  }

  while (true) {
    net_loop();
  }

  // sleep(2);
  // start->vat->messages.push(Msg());

  for (int k = 0; k < processor_count; ++k) {
    burners[k].join();
  }
}
