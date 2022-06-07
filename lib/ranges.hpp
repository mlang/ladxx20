#pragma once

#include <cmath>
#include <numeric>
#include <numbers>
#include <ranges>
#include <tuple>

namespace lad::ranges {

using std::ranges::views::iota;

struct frame {
  int num{};
  std::size_t rate{1};
  double factor{1.0};
  operator double() const { return factor * num / rate; }
  double time() const { return static_cast<double>(num) / rate; }

  constexpr std::tuple<std::size_t, double> range_index(std::size_t sample_rate) const {
    const auto gcd = std::gcd(sample_rate, rate);
    const auto n = (sample_rate / gcd) * num;
    const auto d = rate / gcd;
    const auto phase = factor * n / d;
    const std::size_t index = phase;
    return { index, phase - index };
  }
};

constexpr auto sample_rate(std::size_t rate) {
  return std::ranges::views::transform(
    [rate](int num) -> frame { return {num, rate}; }
  );
}

auto frequency(double value) {
  return std::ranges::views::transform(
    [value](frame pos) -> frame {
      pos.factor = value;
      return pos;
    }
  );
}

template<class T>
concept sized_random_access_range =
  std::ranges::random_access_range<T> && std::ranges::sized_range<T>;

template<sized_random_access_range Range>
auto play(Range&& range, std::size_t rate = 0) {
  using value_type = std::ranges::range_value_t<Range>;
  if (!rate) rate = range.size();
  return std::ranges::views::transform(
    [&range, rate, size = range.size()](frame pos) -> value_type {
      auto [index, offset] = pos.range_index(rate);
      index %= size;
      if (size - index == 1) [[unlikely]] {
        return std::lerp(range.back(), range.front(), offset);
      }
      return std::lerp(range[index], range[index + 1], offset);
    }
  );
}

constexpr auto one_second(int size) {
  return iota(0, size) | sample_rate(size);
}

constexpr auto accurate_sin = std::ranges::views::transform(
  [](frame pos) -> double {
    return std::sin(std::numbers::pi_v<double> * 2.0 * pos);
  }
);

}
