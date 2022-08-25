#include "system.h"
#include "hylic.h"
#include "hylic_ast.h"
#include "hylic_typesolver.h"
#include "core/kernel.h"
#include "other.h"

std::map<std::string, SystemModule> system_module_imports = {
    {"sys►monad", SystemModule::Monad},
    {"sys►io", SystemModule::Io},
    {"sys►net", SystemModule::Net},
};

std::map<SystemModule, std::vector<std::string>> system_module_paths = {
    {SystemModule::Monad, {"sys/monad.po", "Monad"}},
    {SystemModule::Io, {"sys/io.po", "Io"}},
    {SystemModule::Net, {"sys/net.po", "Net"}}
    };

bool is_system_module(std::string import_string) {
  return system_module_imports.find(import_string) != system_module_imports.end();
}

SystemModule system_import_to_enum(std::string str) {
  return system_module_imports[str];
}

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
    if (!exact_match(func_def->ctype, file_ent->functions[func_name]->ctype)) {
      throw TypesolverException("", 0, 0, "Return type does not match internal implementation in " + system_module_paths[mod][1] + "::" + func_name + ". Got " + ctype_to_string(&func_def->ctype) + " in internal method, but " + ctype_to_string(&file_ent->functions[func_name]->ctype) + " in file.");
    }

    if (func_def->param_types.size() != file_ent->functions[func_name]->param_types.size() || func_def->args.size() != file_ent->functions[func_name]->args.size()) {
      throw TypesolverException("", 0, 0, "Number of params/args do not match in " + system_module_paths[mod][1] + ", " + func_name);
    }
    for (int i = 0; i < func_def->param_types.size(); i++)  {
      if (!exact_match(*func_def->param_types[i], *file_ent->functions[func_name]->param_types[i])) {
        print_ctype(*func_def->param_types[i]);
        print_ctype(*file_ent->functions[func_name]->param_types[i]);
        throw PleromaException(std::string("Mismatch between types in function " + func_name + ":") .c_str());
      }
      if (func_def->args[i] != file_ent->functions[func_name]->args[i]) {
        throw TypesolverException("", 0, 0, "Arg names do not match internal implementation in " + system_module_paths[mod][1] + ", " + func_name);
      }
    }
    file_ent->functions[func_name] = func_def;
  }

  typesolve(program);

  return program;
}
