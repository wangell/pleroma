#include <locale>
#include <mutex>
#include <stdio.h>
#include <map>
#include <queue>
#include <unistd.h>
#include <vector>
#include "hylic.h"
#include "hylic_ast.h"
#include "hylic_eval.h"
#include "pleroma.h"

#include <thread>

#include "netcode.h"

//const auto processor_count = std::thread::hardware_concurrency();
const auto processor_count = 1;
const int MAX_STEPS = 3;

std::mutex mtx;
std::map<int, Vat *> vats;

PleromaNode this_pleroma_node;

struct VqNode {
  VqNode* next;

  Vat* vat;

  bool claimed = false;
};

VqNode *queue;

void process_vq() {
  while (true) {
    // Get the next available vat
    mtx.lock();

    if (!queue) {
      mtx.unlock();
      continue;
    }

    VqNode* start = queue;
    while (queue->claimed) {
      queue = queue->next;

      if (start == queue) {
        mtx.unlock();
        break;
      }
    }
    VqNode *our_node = queue;
    our_node->claimed = true;
    mtx.unlock();

    Vat *our_vat = our_node->vat;

    // Take a number of steps
    for (int k = 0; k < 1; ++k) {
      our_vat->message_mtx.lock();
      ////printf("Processing vat %d for step %d\n", our_vat->id, our_vat->run_n);

      while (!our_vat->messages.empty()) {
        Msg m = our_vat->messages.front();
        our_vat->messages.pop();

        print_msg(&m);

        EvalContext context;
        Scope scope;

        //printf("Looking for entity %d in vat %d but couldn't find it in our vat.\n", m.entity_id, m.vat_id);
        //printf("Total entities in vat %lu\n", our_vat->entities.size());
        assert(our_vat->entities.find(m.entity_id) != our_vat->entities.end());

        scope.table = our_vat->entities.find(m.entity_id)->second->file_scope;

        EntityRefNode blah;
        blah.entity_id = 0;
        blah.node_id = 0;
        blah.vat_id = 0;

        scope.table["self"] = &blah;

        context.vat = our_vat;
        context.entity = our_vat->entities.find(m.entity_id)->second;
        context.scope = &scope;

        //printf(" vat mess %d\n", m.entity_id);

        // Return vs call
        if (m.response) {
          // If we didn't setup a promise to resolve, then ignore the result
          if (our_vat->promises.find(m.promise_id) != our_vat->promises.end() && our_vat->promises[m.promise_id].callback) {
            our_vat->promises[m.promise_id].results = m.values;
            eval_promise_local(&context, our_vat->entities.find(m.entity_id)->second, &our_vat->promises[m.promise_id]);
          }
        } else {
          //auto result = eval(&context, eval_func_local(&context, our_vat->entities.find(m.entity_id)->second, m.function_name, {}));
          std::vector<AstNode*> args;

          for (int zz = 0; zz < m.values.size(); ++zz) {
            args.push_back(m.values[zz]);
          }

          auto result = eval_func_local(&context, our_vat->entities.find(m.entity_id)->second, m.function_name, args);
          Msg response_m;
          response_m.entity_id = m.src_entity_id;
          response_m.vat_id = m.src_vat_id;
          response_m.node_id = m.src_node_id;
          response_m.response = true;

          response_m.src_entity_id = m.entity_id;
          response_m.src_vat_id = m.vat_id;
          response_m.src_node_id = m.node_id;
          response_m.promise_id = m.promise_id;

          // All return values are singular - we use tuples to represent multiple return values
          // FIXME might not work if we handle tuples differently
          response_m.values.push_back((ValueNode *)result);

          if (m.function_name != "main") {
            our_vat->out_messages.push(response_m);
          }
        }
      }

      net_mtx.lock();
      while (!our_vat->out_messages.empty()) {
        //printf("sending msg\n");
        Msg m = our_vat->out_messages.front();
        our_vat->out_messages.pop();
        if (m.node_id == this_pleroma_node.node_id) {
          our_vat->messages.push(m);
        } else {
          net_queue.push(m);
        }
      }
      net_mtx.unlock();

      sleep(1);
      our_vat->message_mtx.unlock();

      our_vat->run_n++;
    }

    // Place VAT back in queue
    //printf("processed messages for vat\n");
    mtx.lock();
    our_node->claimed = false;
    mtx.unlock();
  }
}

void inoculate_pleroma(std::map<std::string, AstNode*> program, EntityDef* entity_def) {
  VqNode *c = new VqNode;
  c->claimed = false;
  c->next = c;
  c->vat = new Vat;

  EvalContext context;
  context.entity = nullptr;
  context.vat = c->vat;
  context.node = &this_pleroma_node;

  Entity *ent = create_entity(&context, entity_def);
  ent->file_scope = program;

  c->vat->entities[0] = ent;

  vats[0] = c->vat;
  queue = c;
}

int main(int argc, char **argv) {

  setlocale(LC_ALL, "");
  auto program = load_file("examples/kernel.po");

  auto kernel_program = (EntityDef *)program["Kernel"];

  inoculate_pleroma(program, kernel_program);

  Msg m;
  m.entity_id = 0;
  m.function_name = "main";
  m.node_id = 0;

  m.src_entity_id = -1;
  m.src_node_id = -1;
  m.src_vat_id = -1;
  m.promise_id = -1;

  m.values.push_back((ValueNode*)make_number(99));

  queue->vat->messages.push(m);

  init_network();
  setup_server();

  if (argc > 1) {
    connect_client(get_address(std::string(argv[1])));
  }

  std::thread burners[processor_count];

  for (int k = 0; k < processor_count; ++k) {
    burners[k] = std::thread(process_vq);
  }


  while (true) {
    net_loop();
  }

  //sleep(2);
  //start->vat->messages.push(Msg());

  for (int k = 0; k < processor_count; ++k) {
    burners[k].join();
  }
}
