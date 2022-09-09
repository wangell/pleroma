#include "amoeba.h"
#include "SDL_video.h"
#include "ffi.h"
#include <SDL2/SDL.h>

struct AmoebaWindow {
  u32 window_id = 0;

  SDL_Window* sdl_window;
};

u32 base_window_id = 0;
std::map<u32, AmoebaWindow*> windows;

void refresh_window(AmoebaWindow* window) {
  SDL_UpdateWindowSurface(window->sdl_window);
}

AstNode *amoeba_init(EvalContext *context, std::vector<AstNode *> args) {
  printf("HERE!\n");

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("didn't work\n");
    exit(1);
  }
  return make_nop();
}

AstNode *amoeba_window(EvalContext *context, std::vector<AstNode *> args) {
  AmoebaWindow *window = new AmoebaWindow;
  window->window_id = base_window_id;
  base_window_id++;
  window->sdl_window = SDL_CreateWindow("SDL2 Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 680, 480, 0);

  assert(window->sdl_window);

  refresh_window(window);

  return make_number(window->window_id);
}

AstNode *amoeba_close(EvalContext *context, std::vector<AstNode *> args) {
  SDL_Delay(2000);
  //SDL_DestroyWindow();
  SDL_Quit();
  return make_number(0);
}

AstNode *amoeba_create(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

std::map<std::string, AstNode *> load_amoeba() {
  std::map<std::string, FuncStmt *> functions;

  CType test_type;
  test_type.basetype = PType::u8;
  test_type.dtype = DType::Local;

  CType none_type;
  none_type.basetype = PType::None;
  none_type.dtype = DType::Local;
  functions["create"] = setup_direct_call(amoeba_create, "create", {}, {}, none_type);

  functions["init"] = setup_direct_call(amoeba_init, "init", {}, {}, test_type);
  functions["window"] = setup_direct_call(amoeba_window, "window", {}, {}, test_type);
  functions["close"] = setup_direct_call(amoeba_close, "close", {}, {}, test_type);

  return {
    {"Amoeba", make_actor(nullptr, "Amoeba", functions, {}, {}, {}, {})}}
  ;
}
