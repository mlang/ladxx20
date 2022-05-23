#include <filesystem>
#include <span>
#include <vector>

namespace lad {
  class buffer {
    std::size_t channels = 1;
    std::size_t rate = 0;
    std::vector<float> interleaved;

  public:
    buffer() = default;
    buffer(size_t frames, size_t rate, size_t channels = 1, float value = 0.f);
    buffer(std::filesystem::path);

    size_t frames_per_second() const noexcept { return rate; }
    size_t channel_count() const noexcept { return channels; }
    size_t frame_count() const noexcept { return interleaved.size() / channels; }

    std::span<const float> operator[](size_t index) const {
      return { std::next(interleaved.begin(), index * channels), channels };
    }
    std::span<float> operator[](size_t index) {
      return { std::next(interleaved.begin(), index * channels), channels };
    }
  };
}
