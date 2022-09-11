#include "amoeba.h"
#include "SDL_video.h"
#include "ffi.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

struct AmoebaWindow {
  u32 window_id = 0;

  SDL_Window* sdl_window;
};

u32 base_window_id = 0;
std::map<u32, AmoebaWindow*> windows;

void refresh_window(AmoebaWindow* window) {
  SDL_UpdateWindowSurface(window->sdl_window);
}

void drawText
(AmoebaWindow *window, std::string string,
int size, int x, int y,
unsigned char fR, unsigned char fG, unsigned char fB,
unsigned char bR, unsigned char bG, unsigned char bB)
{

  TTF_Font* font = TTF_OpenFont("Arial.ttf", size);

  if (!font) {
    assert(false);
  }

  SDL_Color foregroundColor = { fR, fG, fB };
  SDL_Color backgroundColor = { bR, bG, bB };

  SDL_Surface* textSurface
  = TTF_RenderText_Shaded
  (font, string.c_str(), foregroundColor, backgroundColor);

  SDL_Rect textLocation = { x, y, 0, 0 };

  SDL_Surface* screen = SDL_GetWindowSurface(window->sdl_window);

  SDL_BlitSurface(textSurface, NULL, screen, &textLocation);

  SDL_FreeSurface(textSurface);

  TTF_CloseFont(font);

  refresh_window(window);
}

AstNode *amoeba_init(EvalContext *context, std::vector<AstNode *> args) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("didn't work\n");
    exit(1);
  }

  TTF_Init();

  return make_nop();
}

AstNode *amoeba_window(EvalContext *context, std::vector<AstNode *> args) {
  AmoebaWindow *window = new AmoebaWindow;
  window->window_id = base_window_id;
  base_window_id++;
  window->sdl_window = SDL_CreateWindow("SDL2 Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 680, 480, 0);
  windows[window->window_id] = window;

  cfs(context).entity->data["window-id"] = make_number(window->window_id);

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

AstNode *amoeba_write(EvalContext *context, std::vector<AstNode *> args) {
  auto window_id = ((NumberNode*)cfs(context).entity->data["window-id"])->value;

  drawText(windows[window_id], "test", 24, 0, 0, 255, 255, 255, 1, 1, 1);

  return make_number(0);
}

std::map<std::string, AstNode *> load_amoeba() {
  std::map<std::string, FuncStmt *> functions;

  CType *str_type = new CType;
  str_type->basetype = PType::str;
  str_type->dtype = DType::Local;

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
  functions["write"] = setup_direct_call(amoeba_write, "write", {"text"}, {str_type}, none_type);

  return {
    {"Amoeba", make_actor(nullptr, "Amoeba", functions, {}, {}, {}, {})}}
  ;
}
