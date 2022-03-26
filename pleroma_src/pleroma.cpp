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
      printf("Processing vat %d for step %d\n", our_vat->id, our_vat->run_n);

      while (!our_vat->messages.empty()) {
        our_vat->messages.pop();
        printf("got message\n");
        eval_func(our_vat->entities[0]->entity_def->functions["main"], {});
      }
      our_vat->message_mtx.unlock();


      our_vat->run_n++;
      sleep(1);
    }

    // Place VAT back in queue
    printf("processed messages for vat\n");
    mtx.lock();
    our_node->claimed = false;
    mtx.unlock();
  }
}

void inoculate_pleroma(EntityDef* entity_def) {
  Entity* ent = create_entity(entity_def);

  VqNode *c = new VqNode;
  c->claimed = false;
  c->next = c;
  c->vat = new Vat;

  c->vat->entities[0] = ent;

  vats[0] = c->vat;
  queue = c;
}

int main(int argc, char **argv) {

  setlocale(LC_ALL, "");
  auto program = load_file("examples/kernel.po");

  auto kernel_program = (EntityDef *)program["Kernel"];

  inoculate_pleroma(kernel_program);

  init_network();
  setup_server();

  if (argc > 1) {
    connect_client(get_address(std::string(argv[1])));
  }

  std::thread burners[processor_count];

  for (int k = 0; k < processor_count; ++k) {
    burners[k] = std::thread(process_vq);
  }

  sleep(2);
  Msg m;
  m.actor_id = 0;
  m.function_id = 0;
  queue->vat->messages.push(m);

  while (true) {
    net_loop();
  }

  //sleep(2);
  //start->vat->messages.push(Msg());

  for (int k = 0; k < processor_count; ++k) {
    burners[k].join();
  }
}
