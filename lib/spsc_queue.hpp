#include <array>
#include <atomic>
#include <cassert>
#include <functional>
#include <type_traits>
#include <variant>

namespace lad {
	
// Producer destroys
template<typename Type, std::size_t Capacity>
class spsc_queue {
public:
  using value_type = Type;

  ~spsc_queue() {
    for (std::size_t i = 0; i != Capacity; i++) {
      maybe_destroy(i);
    }
  }

  template<typename... Args> bool try_emplace(Args&&... args) {
    if (is_full()) return false;

    maybe_destroy(write_index);
    ::new(&data[write_index]) value_type(std::forward<Args>(args)...);
    initialized[write_index] = true;
    write_index.store((write_index + 1) % Capacity, std::memory_order_relaxed);

    return true;
  }

  template<typename Callback> bool try_consume(Callback&& callback) {
    if (is_empty()) return false;

    assert(initialized[read_index]);

    if (std::invoke(std::forward<Callback>(callback), *get(read_index))) {
      read_index.store((read_index + 1) % Capacity, std::memory_order_relaxed);
    }

    return true;
  }

private:
  std::aligned_storage_t<sizeof(value_type)> data[Capacity];
  std::array<bool, Capacity> initialized{};
  value_type *get(std::size_t index) {
    return std::launder(reinterpret_cast<value_type*>(&data[index]));
  }
  void maybe_destroy(std::size_t index) {
    if (initialized[index]) {
      std::destroy_at(get(index));
      initialized[index] = false;
    }
  }
  std::atomic_size_t read_index{}, write_index{};
  bool is_empty() const {
    return read_index == write_index.load(std::memory_order_relaxed);
  }
  bool is_full() const {
    return (write_index + 1) % Capacity == read_index.load(std::memory_order_relaxed);
  }
};

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

template<typename Visitor, typename Variant, std::size_t Capacity>
bool try_visit(Visitor&& visitor, spsc_queue<Variant, Capacity> &queue) {
  return queue.try_consume(
    [&visitor](auto&& variant) {
      return visit(
        std::forward<Visitor>(visitor),
        std::forward<decltype(variant)>(variant)
      );
    }
  );
}

}
