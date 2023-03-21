#include "hosted_irq.h"
#include <SDL2/SDL.h>
#include <unistd.h>
#include "hylic.h"
#include "hylic_ast.h"
#include "netcode.h"

void start() {
}

void loop_keyboard() {
  while (true) {
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
      break;
    }
    SDL_Delay(1000);
  }
  printf("Confirmed, SDL initialized. Starting loop\n");
  SDL_Event event;
  while (true) {
    SDL_WaitEvent(&event);
    if (event.type == SDL_KEYDOWN) {
      Msg m;
      m.node_id = monad_ref->node_id;
      m.vat_id = monad_ref->vat_id;
      m.entity_id = monad_ref->entity_id;
      m.src_node_id = -1;
      m.src_vat_id = -1;
      m.src_entity_id = -1;
      m.function_name = "irq-handler";
      m.values.push_back((ValueNode*)make_number(1));
      m.values.push_back((ValueNode *)make_number(event.key.keysym.sym));
      net_out_queue.enqueue(m);
    }
  }
}

void loop_timer() {
  sleep(1);
  printf("Timer IRQ!\n");
}
