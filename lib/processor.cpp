#include "processor.hpp"
#include <jack/jack.h>
#include <cassert>
#include <vector>

namespace lad {

struct processor::implementation {
  processor &interface;
  jack_client_t *client;
  std::vector<jack_port_t *> in;
  std::vector<jack_port_t *> out;
  std::vector<std::span<const float>> in_span, in_block_span;
  std::vector<std::span<float>> out_span, out_block_span;
  std::size_t block_size;
  std::size_t sample_rate;

  implementation(processor &, const char *client_name, std::size_t in_count, std::size_t out_count, std::size_t block_size);
  ~implementation() {
    jack_deactivate(client);
    for (auto port: in) jack_port_unregister(client, port);
    in.clear();
    for (auto port: out) jack_port_unregister(client, port);
    out.clear();
    jack_client_close(client);
  }

  int process(jack_nframes_t frames) {
    size_t port_index = 0;
    for (auto port: in)
      in_span[port_index++] = std::span(
        static_cast<const float *>(jack_port_get_buffer(port, frames)), frames
      );
    port_index = 0;
    for (auto port: out)
      out_span[port_index++] = std::span(
        static_cast<float *>(jack_port_get_buffer(port, frames)), frames
      );
    jack_nframes_t frame = 0;
    while (frame + block_size <= frames) {
      std::transform(in_span.begin(), in_span.end(), in_block_span.begin(),
        [&](auto span) { return span.subspan(frame, block_size); }
      );
      std::transform(out_span.begin(), out_span.end(), out_block_span.begin(),
        [&](auto span) { return span.subspan(frame, block_size); }
      );

      interface.process({{in_block_span}, {out_block_span}});

      frame += block_size;
    }

    assert(frame == frames);

    return 0;
  }
};

namespace jack {
namespace {

int process(jack_nframes_t frames, void *instance) {
  return static_cast<processor::implementation *>(instance)->process(frames);
}

}
}

processor::implementation::implementation(
  processor &interface,
  const char *client_name,
  std::size_t in_count, std::size_t out_count,
  std::size_t block_size
)
: interface { interface }
, client { [client_name] {
    jack_status_t status;
    if (auto handle = jack_client_open(client_name, JackNullOption, &status)) {
      return handle;
    }
    throw std::runtime_error("Error initializing JACK");
  }()
}
, in_span(in_count)
, in_block_span(in_count)
, out_span(out_count)
, out_block_span(out_count)
, block_size { block_size? block_size: jack_get_buffer_size(client) }
, sample_rate { jack_get_sample_rate(client) }
{
  assert(jack_get_buffer_size(client) % block_size == 0);
  in.reserve(in_count);
  for (std::size_t i = 0; i < in_count; i += 1) {
    const auto name = "in_" + std::to_string(i + 1);
    static constexpr auto flags = JackPortIsInput | JackPortIsTerminal;
    in.push_back(jack_port_register(client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, flags, 0));
  }
  out.reserve(out_count);
  for (std::size_t i = 0; i < out_count; i += 1) {
    const auto name = "out_" + std::to_string(i + 1);
    static constexpr auto flags = JackPortIsOutput | JackPortIsTerminal;
    out.push_back(jack_port_register(client, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, flags, 0));
  }
  jack_set_process_callback(client, jack::process, this);
}

processor::processor (
  std::string name, std::size_t in, std::size_t out, std::size_t block_size
)
: impl {
    std::make_unique<implementation>(
      *this, name.c_str(), in, out, block_size
    )
  }
{
  assert(in > 0 || out > 0);
}

processor::~processor() = default;

void processor::connect_audio_out() {
  std::size_t index{};
  for (auto port: impl->out) {
    auto name = "system:playback_" + std::to_string(++index);
    if (auto error_code = jack_connect(impl->client, jack_port_name(port), name.c_str())) {
      throw std::runtime_error("Jack port connection failed");
    }
  }
}

void processor::start() {
  if (auto error_code = jack_activate(impl->client))
    throw std::runtime_error("JACK activation failed: " + std::to_string(error_code));
}

std::size_t processor::frames_per_second() const {
  return impl->sample_rate;
}

}
