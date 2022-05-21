#include "buffer.hpp"
#include <sndfile.hh>

namespace lad {
	
buffer::buffer(size_t frames, size_t rate, size_t channels, float value)
: channels{channels}
, rate{rate}
, interleaved(frames * channels, value)
{
}

buffer::buffer(std::filesystem::path path)
{
  SndfileHandle file(path);
  interleaved.resize(file.channels() * file.frames());
  channels = file.channels();
  rate = file.samplerate();
  file.readf(interleaved.data(), file.frames());
}

}
