#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
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
  , fallback { [](auto line) { std::cerr << line << std::endl; return true; } }
  {}
  simple_interpreter(commands_type commands, fallback_type fallback)
  : commands{commands}
  , fallback{fallback}
  {}
  void operator()(std::istream &in) const {
    std::string line;
    while (std::getline(in, line)) {
      using namespace std::ranges;
      static constexpr auto isspace = [](unsigned char c) { return std::isspace(c); };
      auto begin = find_if_not(line, isspace);
      auto iter = find_if(begin, line.end(), isspace);
      auto cmd = commands.find({begin, iter});
      if (cmd != commands.end()) {
        std::stringstream rest(std::string(iter, line.end()));
        std::string arg;
        std::vector<decltype(arg)> args;
        while (rest >> std::quoted(arg)) args.push_back(arg);

        auto &function = cmd->second;
        if (!function(std::move(args))) break;
      } else {
        if (!fallback(line)) break;
      }
    }
  }
  void operator()(std::filesystem::path path) const {
    std::ifstream file(path);
    if (file) operator()(file);
  }

  struct command {
    struct source {
      simple_interpreter const &interpret;
      bool operator()(args_type args) const {
        if (!args.empty()) {
          auto path = std::filesystem::path(args[0]);
          if (exists(path)) {
            interpret(path);
          }
        }
        return true;
      }
    };
    static bool current_path(args_type args) {
      if (args.empty()) {
        std::cout << std::string(std::filesystem::current_path()) << std::endl;
      } else {
        auto path = std::filesystem::path(args[0]);
        if (is_directory(path)) {
          std::filesystem::current_path(path);
        }
      }
      return true;
    }
  };
};

}
