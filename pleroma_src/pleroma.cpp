#include <locale>
#include <mutex>
#include <stdio.h>
#include <map>
#include <queue>
#include <unistd.h>
#include <vector>
#include "hylic.h"
#include "pleroma.h"

#include <thread>

#include "netcode.h"

//const auto processor_count = std::thread::hardware_concurrency();
const auto processor_count = 2;
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
      printf("Processing vat %d for step %d\n", our_vat->id, our_vat->run_n);

      our_vat->message_mtx.lock();
      while (!our_vat->messages.empty()) {
        our_vat->messages.pop();
        printf("got message\n");
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

int main(int argc, char **argv) {

  load_file("kernel.po");

  exit(1);
  init_network();
  setup_server();

  if (argc > 1) {
    connect_client(get_address(std::string(argv[1])));
  }

  VqNode *c = new VqNode;
  c->claimed = false;
  c->next = c;
  c->vat = new Vat;
  vats[0] = c->vat;
  queue = c;

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
