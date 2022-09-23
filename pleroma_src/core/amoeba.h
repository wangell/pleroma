#pragma once

#include "../hylic_ast.h"
#include <SDL2/SDL.h>
#include <map>
#include <string>

std::map<std::string, AstNode *> load_amoeba();

struct AmoebaComponent {
  int x, y;
};

struct TextBox : AmoebaComponent {
  std::string text;
};

struct AmoebaWindow {
  u32 window_id = 0;
  SDL_Window *sdl_window;

  std::vector<AmoebaComponent> components;
};
