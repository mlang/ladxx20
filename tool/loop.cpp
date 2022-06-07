#include <buffer.hpp>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <processor.hpp>
#include <simple_interpreter.hpp>
#include <static_spsc_queue.hpp>

using namespace lad;
using namespace std;

struct loop {
  lad::buffer buffer;
  double bpm;
  float volume = 1.0;
};

struct set_bpm { double bpm; };
struct set_volume { double value; };
using msg_t = std::variant<set_bpm, set_volume>;

struct player final : processor {
  lad::static_spsc_queue<msg_t, 128> incoming;
        double tempo = 120.0;
  float volume = 0.035;
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
  std::size_t total_frames = 0;
  player(size_t out = 2) : processor("null", 0, out) {}
  void start() {
    if (!loops.empty()) set_loop(0);
    processor::start();
  }
  bool operator()(set_bpm const &msg) {
    tempo = msg.bpm;
    set_loop(loop_index);

    return true;
  }
  bool operator()(set_volume const &msg) {
    volume = msg.value;

    return true;
  }
  void process(audio_buffers audio) override {
    while(try_visit(*this, incoming));

    if (loops.size() <= loop_index) {
      for (auto buffer : audio.out)
        ranges::fill(buffer, 0.0);
      return;
    }

    const auto frames = size(audio.out.front());
    for (std::size_t frame : views::iota(decltype(frames){}, frames)) {
      auto const &loop = loops[loop_index];
      std::size_t loop_frame = position;
      const float offset = position - loop_frame;
      std::size_t channel{};
      for (auto buffer : audio.out) {
        array<float, 2> sample;
        sample[0] = loop.buffer[loop_frame][channel % loop.buffer.channel_count()];
        if (loop_frame + 1 < loop.buffer.frame_count()) [[likely]] {
          sample[1] = loop.buffer[loop_frame + 1]
                                 [channel % loop.buffer.channel_count()];
        } else {
          auto const &next_buffer = loops[(loop_index + 1) % loops.size()].buffer;
          sample[1] = next_buffer[0][channel % next_buffer.channel_count()];
        }

        buffer[frame] = volume * loop.volume * lerp(sample[0], sample[1], offset);

        channel += 1;
      }

      position += delta;

      if (position >= loop.buffer.frame_count()) [[unlikely]] {
        position = position - loop.buffer.frame_count();
        set_loop((loop_index + 1) % loops.size());
      }
    }

    total_frames += frames;
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
        audio.incoming.try_emplace(set_bpm{tempo});
      }
    }
    return true;
  };
  interpret.commands.insert({"tempo", tempo});

  auto volume = [&](auto args) {
    if (!args.empty()) {
      auto volume = strtod(args[0].c_str(), nullptr);
      if (volume > 0) {
        audio.incoming.try_emplace(set_volume{volume});
      }
    }
    return true;
  };
  interpret.commands.insert({"volume", volume});

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
