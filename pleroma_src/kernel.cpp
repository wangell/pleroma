#include "kernel.h"
#include "hylic_ast.h"
#include "hylic_eval.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <string>
#include <vector>

std::map<std::string, AstNode *> kernel_map;

AstNode *io_print(EvalContext *context, std::vector<AstNode *> args) {

  auto pval = eval(context, args[0]);

  if (pval->type == AstNodeType::StringNode) {
    printf("%s\n", ((StringNode *)pval)->value.c_str());
  }

  if (pval->type == AstNodeType::NumberNode) {
    printf("%ld\n", ((NumberNode *)pval)->value);
  }

  if (pval->type == AstNodeType::ListNode) {
    printf("%ld\n", ((NumberNode *)pval)->value);
  }

  return make_nop();
}

AstNode *io_readline(EvalContext *context, std::vector<AstNode *> args) {

  std::string user_input;

  std::getline(std::cin, user_input);

  return make_string(user_input);
}

// Context may be unnecessary
AstNode *test_ffi(EvalContext *context, std::vector<AstNode *> args) {

  auto blah = (NumberNode *)args[0];

  printf("FFI demo %ld\n", blah->value);

  return make_number(4);
}

FuncStmt *setup_test() {
  std::vector<AstNode *> body;

  body.push_back(make_foreign_func_call(test_ffi, {make_symbol("sys")}));

  return (FuncStmt *)make_function("main", {"sys"}, body, {});
}

FuncStmt *setup_direct_call(AstNode *(*foreign_func)(EvalContext *,
                                                     std::vector<AstNode *>),
                            std::string name, std::vector<std::string> args,
                            std::vector<CType *> arg_types) {
  std::vector<AstNode *> body;
  std::vector<AstNode *> nu_args;

  for (auto k : args) {
    nu_args.push_back(make_symbol(k));
  }

  body.push_back(make_foreign_func_call(foreign_func, nu_args));

  return (FuncStmt *)make_function(name, args, body, arg_types);
}

void do_test() {}

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
  return make_nop();
}

void load_amoeba() {
  std::map<std::string, FuncStmt *> functions;

  functions["init"] = setup_direct_call(amoeba_init, "init", {}, {});
  functions["window"] = setup_direct_call(amoeba_window, "window", {}, {});
  functions["close"] = setup_direct_call(amoeba_close, "close", {}, {});

  kernel_map["Amoeba"] = make_actor("Amoeba", functions, {});
}

void load_kernel() {
  std::map<std::string, FuncStmt *> functions;
  // functions["main"] = setup_test();
  functions["main"] = setup_direct_call(test_ffi, "main", {"sys"}, {});

  kernel_map["Kernel"] = make_actor("Kernel", functions, {});

  std::map<std::string, FuncStmt *> io_functions;

  io_functions["print"] = setup_direct_call(io_print, "print", {"val"}, {});
  io_functions["readline"] = setup_direct_call(io_readline, "readline", {}, {});

  kernel_map["Io"] = make_actor("Io", io_functions, {});

  load_amoeba();
}
