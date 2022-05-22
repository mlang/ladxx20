#include <filesystem>
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>
#include <unordered_map>

namespace lad {

struct simple_interpreter {
  using args_type = std::vector<std::string>;
  using function_type = std::function<bool(args_type)>;
  using commands_type = std::unordered_map<std::string_view, function_type>;
  commands_type commands;
  using fallback_type = std::function<bool(std::string)>;
  fallback_type fallback;

  simple_interpreter()
  : commands {
      { "source", command::source{*this} },
      { "current_path", command::current_path }
    }
  , fallback{[](auto line) { return true; }} {}
  simple_interpreter(commands_type commands, fallback_type fallback)
  : commands{std::move(commands)}, fallback{std::move(fallback)} {}

  void operator()(std::istream &in) const;
  void operator()(std::filesystem::path path) const;

  struct command {
    struct source {
      simple_interpreter const &interpret;
      bool operator()(args_type args) const;
    };
    static bool current_path(args_type args);
  };
};

}
