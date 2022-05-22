#include <experimental/propagate_const>
#include <memory>
#include <span>
#include <string>

// WISHLIST:
// - A units library for frequency
namespace lad {
  struct processor {
    struct implementation;

    processor(
      std::string name,
      std::size_t in, std::size_t out,
      std::size_t block_size = 0
    );
    virtual ~processor();

    std::size_t frames_per_second() const;
    void start();
    void connect_audio_out();

    struct audio_buffers {
      const std::span<const std::span<const float>> in;
      const std::span<const std::span<float>> out;
    };
  protected:
    virtual void process(audio_buffers) = 0;

  private:
    std::experimental::propagate_const<std::unique_ptr<implementation>> impl;
    friend struct implementation;
  };
}
