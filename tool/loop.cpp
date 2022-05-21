#include <buffer.hpp>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <processor.hpp>
#include <simple_interpreter.hpp>

using namespace lad;
using namespace std;

struct loop {
  lad::buffer buffer;
  double bpm;
};

struct player final : processor {
  double tempo = 120.0;
  float volume = 0.025;
  vector<loop> loops;
  std::size_t loop_index = 0;
  double position = 0;
  double delta = 1.0;
  void set_loop(std::size_t index) {
    loop_index = index;
    auto const &loop = loops[loop_index];
    delta = (loop.buffer.frames_per_second() * tempo) /
            (frames_per_second() * loop.bpm);
  }
  player(size_t out = 2) : processor("null", 0, out) {}
  void start() {
    if (!loops.empty()) set_loop(0);
    processor::start();
  }
  void process(audio_buffers audio) override {
    if (loops.size() <= loop_index) {
      for (auto buffer : audio.out)
        ranges::fill(buffer, 0.0);
      return;
    }

    for (std::size_t frame = 0; frame != size(audio.out.front()); frame += 1) {
      auto const &loop = loops[loop_index];
      std::size_t loop_frame = position;
      float offset = position - loop_frame;
      std::size_t channel{};
      for (auto buffer : audio.out) {
        auto v1 = loop.buffer[loop_frame][channel];
        auto v2 = v1;
        if (loop_frame + 1 == loop.buffer.frame_count()) {
          if (loop_index + 1 < loops.size()) {
            auto const &next_loop = loops[loop_index + 1];
            v2 = next_loop.buffer[0][min(next_loop.buffer.channel_count() - 1,
                                         channel)];
          }
        } else {
          v2 = loop.buffer[loop_frame + 1][channel];
        }
        buffer[frame] = volume * lerp(v1, v2, offset);
        if (channel < loop.buffer.channel_count() - 1) channel += 1;
      }
      position += delta;
      if (position >= loop.buffer.frame_count()) {
        position = position - loop.buffer.frame_count();
        set_loop(loop_index + 1);
      }
    }
  }
};

int main(int argc, char *argv[]) {
  auto const [program, args] = [args = span(argv, argc)] {
    return tuple{filesystem::path(args.front()), args.subspan(1)};
  }();

  auto audio = player();

  auto interpret = simple_interpreter{};

  auto load = [&](auto args) {
    if (args.size() == 2) {
      const double bpm = strtod(args[0].c_str(), nullptr);
      const auto path = filesystem::path(args[1]);
      if (bpm > 0 && exists(path)) {
        audio.loops.push_back({path, bpm});
        auto &loop = audio.loops.back();
        cout << loop.buffer.frame_count() << endl;

        return true;
      }
    }
    return false;
  };
  interpret.commands.insert({"load", load});

  auto start = [&](auto args) {
    audio.start();
    return true;
  };
  interpret.commands.insert({"start", start});

  auto connect = [&](auto args) {
    audio.connect_audio_out();
    return true;
  };
  interpret.commands.insert({"connect", connect});

  auto tempo = [&](auto args) {
    if (!args.empty()) {
      auto tempo = strtod(args[0].c_str(), nullptr);
      if (tempo > 0) {
        audio.tempo = tempo;
      }
    }
    return true;
  };
  interpret.commands.insert({"tempo", tempo});

  if (auto config_home = getenv("XDG_CONFIG_HOME")) {
    interpret(filesystem::path(config_home) / program.filename() / "startup");
  } else if (auto home = getenv("HOME")) {
    interpret(filesystem::path(home) / ".config" / program.filename() / "startup");
  }

  if (!args.empty()) {
    for (auto arg : args) {
      if ("-"s == arg)
        interpret(cin);
      else
        interpret(filesystem::path(arg));
    }
  } else {
    interpret(cin);
  }

  return EXIT_SUCCESS;
}
