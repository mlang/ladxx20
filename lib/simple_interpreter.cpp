#include "simple_interpreter.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace lad {

void simple_interpreter::operator()(std::istream &in) const {
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

void simple_interpreter::operator()(std::filesystem::path path) const {
  std::ifstream file(path);
  if (file) operator()(file);
}

bool simple_interpreter::command::source::operator()(args_type args) const {
  if (!args.empty()) {
    auto path = std::filesystem::path(args[0]);
    if (exists(path)) {
      interpret(path);
    }
  }

  return true;
}

bool simple_interpreter::command::current_path(args_type args) {
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

}
