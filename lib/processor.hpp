#include <experimental/propagate_const>
#include <memory>
#include <span>
#include <string>

namespace lad {
  struct processor {
    struct implementation;

    processor(
      std::string name,
      std::size_t in, std::size_t out,
      std::size_t block_size = 0
    );
    virtual ~processor();

    void connect_audio_out();
    void start();

    std::size_t frames_per_second() const;

    struct audio_buffers {
      std::span<const std::span<const float>> in;
      std::span<const std::span<float>> out;
    };
  protected:
    virtual void process(audio_buffers) = 0;

  private:
    std::experimental::propagate_const<std::unique_ptr<implementation>> impl;
    friend struct implementation;
  };
}
