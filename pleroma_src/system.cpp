#include "system.h"
#include "hylic.h"
#include "hylic_ast.h"
#include "hylic_typesolver.h"
#include "core/kernel.h"
#include "other.h"

std::map<SystemModule, std::vector<std::string>> system_module_paths = {
  {SystemModule::Monad, {"sys/monad.po", "Monad"}}
};

HylicModule *load_system_module(SystemModule mod) {
  HylicModule *program;

  TokenStream *stream = tokenize_file(system_module_paths[mod][0]);

  program = parse(stream);

  // Replace the stubs with our functions
  auto file_ent = (EntityDef *) program->entity_defs[system_module_paths[mod][1]];
  auto sub_ent = (EntityDef*) kernel_map[system_module_paths[mod][1]];

  for (auto &[func_name, func_def] : sub_ent->functions) {
    assert(file_ent->functions.find(func_name) != file_ent->functions.end());
    // Check if they agree on return type
    assert(exact_match(func_def->ctype, file_ent->functions[func_name]->ctype));
    for (int i = 0; i < func_def->param_types.size(); i++)  {
      if (!exact_match(*func_def->param_types[i], *file_ent->functions[func_name]->param_types[i])) {
        throw PleromaException(std::string("Mismatch between types in function " + func_name + ":").c_str());
        print_ctype(*func_def->param_types[i]);
        print_ctype(*file_ent->functions[func_name]->param_types[i]);
      }
      assert(func_def->args[i] == file_ent->functions[func_name]->args[i]);
    }
    file_ent->functions[func_name] = func_def;
  }

  typesolve(program);

  return program;
}
