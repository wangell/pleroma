#include "amoeba.h"
#include "../type_util.h"
#include "SDL_keycode.h"
#include "SDL_render.h"
#include "SDL_stdinc.h"
#include "SDL_video.h"
#include "ffi.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

SDL_Renderer *renderer;
std::string command_text;

// Base
struct MouseState {
  int x, y;
};

u32 base_window_id = 0;
std::map<u32, AmoebaWindow*> windows;

std::vector<AmoebaComponent> components;

void refresh_window(AmoebaWindow* window) {
  SDL_UpdateWindowSurface(window->sdl_window);
}

void render_window() {
}

MouseState mouse_xy() {
   int x, y;
   Uint32 buttons = SDL_GetMouseState(&x, &y);
   return {
     x, y
   };
}

// Hylic API

void drawText(AmoebaWindow *window, std::string string, int size, int x, int y, unsigned char fR, unsigned char fG, unsigned char fB, unsigned char bR, unsigned char bG, unsigned char bB) {

  TTF_Font* font = TTF_OpenFont("noto.ttf", size);

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

void draw_text(SDL_Renderer* renderer, std::string text, int x, int y) {
  TTF_Font *font = TTF_OpenFont("noto.ttf", 20);

  if (!font) {
    assert(false);
  }

  SDL_Color foregroundColor = {0, 0, 0, 255};
  SDL_Color backgroundColor = {0, 0, 0, 0};

  SDL_Surface *textSurface = TTF_RenderText_Blended_Wrapped(font, text.c_str(), foregroundColor, 100);

  SDL_Rect textLocation = {x, y, textSurface->w, textSurface->h};

  SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, textSurface);
  //SDL_SetTextureAlphaMod(tex, backgroundColor.a);

  SDL_RenderCopy(renderer, tex, NULL, &textLocation);

  SDL_FreeSurface(textSurface);
  SDL_DestroyTexture(tex);

  TTF_CloseFont(font);
}

void render_commandbox(SDL_Renderer* renderer) {
  SDL_Rect r = {int(1920.0 / 2.0f - 200), 1200 - 100, 400, 50};
  SDL_SetRenderDrawColor(renderer, 0, 0, 150, 50);
  SDL_RenderFillRect(renderer, &r);
  if (!command_text.empty()) {
    draw_text(renderer, command_text, int(1920.0 / 2.0f), (1200 - 100));
  }
}

void render() {
  SDL_SetRenderDrawColor(renderer, 234, 245, 253, 255);
  SDL_RenderClear(renderer);

  draw_text(renderer, "Pleroma", 0, 0);

  render_commandbox(renderer);

  SDL_RenderPresent(renderer);
}

AstNode *amoeba_init(EvalContext *context, std::vector<AstNode *> args) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    panic("Failed to initialize video system.");
  }

  if (TTF_Init() < 0) {
    panic("Failed to initialize TTF system.");
  }

  EntityRefNode* rfn = (EntityRefNode*)cfs(context).entity->data["mnd"];
  eval_message_node(context, rfn, CommMode::Async, "subscribe-irq", {make_number(1)});

  auto window = SDL_CreateWindow("SDL2 Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 680, 480, SDL_WINDOW_FULLSCREEN_DESKTOP);
  renderer = SDL_CreateRenderer(window, -1, 0);

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  command_text = "test";

  render();

  return make_number(0);
}

AstNode *amoeba_super_window(EvalContext *context, std::vector<AstNode *> args) {
  AmoebaWindow *window = new AmoebaWindow;
  window->window_id = base_window_id;
  base_window_id++;
  window->sdl_window = SDL_CreateWindow("SDL2 Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 680, 480, 0);
  windows[window->window_id] = window;

  assert(window->sdl_window);

  refresh_window(window);

  auto env = eval(context, make_create_entity("AmoebaWindow", false));
  auto ent_ref = (EntityRefNode*)env;

  context->vat->entities[ent_ref->entity_id]->data["window-id"] = make_number(window->window_id);

  return env;
}

AstNode *amoeba_close(EvalContext *context, std::vector<AstNode *> args) {
  SDL_Delay(10000);
  //SDL_DestroyWindow();
  SDL_Quit();
  return make_number(0);
}

AstNode *amoeba_create(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

AstNode *super_window_create(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

AstNode *super_window_close(EvalContext *context, std::vector<AstNode *> args) {
  return make_number(0);
}

AstNode *super_window_write(EvalContext *context, std::vector<AstNode *> args) {

  auto window_id = ((NumberNode*)cfs(context).entity->data["window-id"])->value;

  drawText(windows[window_id], "test", 24, 0, 0, 255, 255, 255, 0, 0, 0);

  refresh_window(windows[window_id]);

  return make_number(0);
}

AstNode *amoeba_write(EvalContext *context, std::vector<AstNode *> args) {

  return make_number(0);
}

AstNode *amoeba_handle_input(EvalContext *context, std::vector<AstNode *> args) {
  auto key_val = ((NumberNode*)args[0])->value;

  printf("got key %ld\n", key_val);

  if (key_val == SDLK_ESCAPE) {
    SDL_Quit();
  }

  if (key_val >= SDLK_a && key_val <= SDLK_z) {
    command_text.push_back('a' + (key_val - SDLK_a));
  }

  if (key_val == SDLK_RETURN) {
    printf("Running command %s\n", command_text.c_str());
    command_text.clear();
  }

  render();

  return make_number(0);
}

std::map<std::string, AstNode *> load_amoeba() {
  CType window_type;
  window_type.basetype = PType::Entity;
  window_type.entity_name = "AmoebaWindow";
  window_type.dtype = DType::Far;

  std::map<std::string, FuncStmt *> functions = {
    {"create", setup_direct_call(amoeba_create, "create", {}, {}, *void_t())},
    {"handle-input", setup_direct_call(amoeba_handle_input, "handle-input", {"data"}, {lu8()}, *void_t())}
  };

  functions["init"] = setup_direct_call(amoeba_init, "init", {}, {}, *lu8());
  //functions["window"] = setup_direct_call(amoeba_window, "window", {}, {}, window_type);
  //functions["super-window"] = setup_direct_call(amoeba_super_window, "window", {}, {}, window_type);
  functions["close"] = setup_direct_call(amoeba_close, "close", {}, {}, *lu8());
  //functions["write"] = setup_direct_call(amoeba_write, "write", {"text"}, {str_type}, none_type);

  //std::map<std::string, FuncStmt *> window_functions;
  //window_functions["create"] = setup_direct_call(window_create, "window", {}, {}, *void_t());
  //window_functions["write"] = setup_direct_call(window_write, "write", {"text"}, {lstr()}, *void_t());
  //window_functions["close"] = setup_direct_call(window_close, "close", {}, {}, *lu8());

  return {
    {"Amoeba", make_actor(nullptr, "Amoeba", functions, {}, {}, {}, {})},
    //{"AmoebaWindow", make_actor(nullptr, "AmoebaWindow", window_functions, {}, {}, {}, {})}
  };
}
