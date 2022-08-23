#include "amoeba.h"
#include "ffi.h"
#include <SDL2/SDL.h>

AstNode *amoeba_init(EvalContext *context, std::vector<AstNode *> args) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("didn't work\n");
    exit(1);
  }
  return make_nop();
}

AstNode *amoeba_window(EvalContext *context, std::vector<AstNode *> args) {
  printf("wendow\n");
  SDL_Window *window = SDL_CreateWindow("SDL2 Window", SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED, 680, 480, 0);
  SDL_Surface *window_surface = SDL_GetWindowSurface(window);

  if (!window_surface) {
    exit(1);
  }

  SDL_UpdateWindowSurface(window);

  SDL_Delay(5000);
  return make_number(4);
}

AstNode *amoeba_close(EvalContext *context, std::vector<AstNode *> args) {
  //SDL_DestroyWindow();
  SDL_Quit();
  return make_number(0);
}

void load_amoeba() {
  std::map<std::string, FuncStmt *> functions;

  CType test_type;
  test_type.basetype = PType::u8;

  functions["init"] = setup_direct_call(amoeba_init, "init", {}, {}, test_type);
  functions["window"] = setup_direct_call(amoeba_window, "window", {}, {}, test_type);
  functions["close"] = setup_direct_call(amoeba_close, "close", {}, {}, test_type);

  //kernel_map["Amoeba"] = make_actor("Amoeba", functions, {});
}
