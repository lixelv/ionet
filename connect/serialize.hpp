#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

template <typename T>
concept tuple_like = requires {
  typename std::tuple_size<std::remove_cvref_t<T>>::type;
} && !requires(T t) { t.begin(); };

template <typename T> void write_data(std::vector<char> &out, const T &obj);
template <typename T> void read_data(std::span<const char> &in, T &obj);

template <typename T>
  requires requires(const T t, std::vector<char> &v) { t.serialize(v); }
void write_data(std::vector<char> &out, const T &obj) {
  obj.serialize(out);
}

template <typename T>
  requires std::is_arithmetic_v<T>
void write_data(std::vector<char> &out, const T &obj) {
  size_t offset = out.size();
  out.resize(offset + sizeof(T));

  if constexpr (std::endian::native == std::endian::big && sizeof(T) > 1) {
    T copy = obj;
    char *ptr = reinterpret_cast<char *>(&copy);
    std::reverse(ptr, ptr + sizeof(T));
    std::memcpy(out.data() + offset, ptr, sizeof(T));
  } else {
    std::memcpy(out.data() + offset, &obj, sizeof(T));
  }
}

inline void write_data(std::vector<char> &out, const std::string &obj) {
  write_data(out, static_cast<uint64_t>(obj.size()));
  out.insert(out.end(), obj.begin(), obj.end());
}

template <typename T>
void write_data(std::vector<char> &out, const std::vector<T> &obj) {
  write_data(out, static_cast<uint64_t>(obj.size()));
  if constexpr (std::is_trivially_copyable_v<T> &&
                std::endian::native == std::endian::little) {
    const char *ptr = reinterpret_cast<const char *>(obj.data());
    out.insert(out.end(), ptr, ptr + (obj.size() * sizeof(T)));
  } else {
    for (const auto &item : obj) {
      write_data(out, item);
    }
  }
}

template <typename T, size_t N>
void write_data(std::vector<char> &out, const std::array<T, N> &obj) {
  for (const auto &item : obj)
    write_data(out, item);
}

template <typename T>
void write_data(std::vector<char> &out, const std::set<T> &obj) {
  write_data(out, static_cast<uint64_t>(obj.size()));
  for (const auto &item : obj)
    write_data(out, item);
}

template <typename K, typename V>
void write_data(std::vector<char> &out, const std::map<K, V> &obj) {
  write_data(out, static_cast<uint64_t>(obj.size()));
  for (const auto &[key, val] : obj) {
    write_data(out, key);
    write_data(out, val);
  }
}

template <typename Tuple, std::size_t... I>
void write_tuple_impl(std::vector<char> &out, const Tuple &obj,
                      std::index_sequence<I...>) {
  (write_data(out, std::get<I>(obj)), ...);
}

template <typename Tuple>
  requires tuple_like<Tuple>
void write_data(std::vector<char> &out, const Tuple &obj) {
  constexpr std::size_t N = std::tuple_size_v<std::remove_cvref_t<Tuple>>;
  write_tuple_impl(out, obj, std::make_index_sequence<N>{});
}

template <typename... Args>
void write_data(std::vector<char> &out, const std::variant<Args...> &obj) {
  write_data(out, static_cast<uint64_t>(obj.index()));
  std::visit([&](auto &&arg) { write_data(out, arg); }, obj);
}

inline void check_size(std::span<const char> &in, size_t needed) {
  if (in.size() < needed)
    throw std::runtime_error("Serialization error: buffer underrun");
}

template <typename T>
  requires requires(T t, std::span<const char> &s) { t.deserialize(s); }
void read_data(std::span<const char> &in, T &obj) {
  obj.deserialize(in);
}

template <typename T>
  requires std::is_arithmetic_v<T>
void read_data(std::span<const char> &in, T &obj) {
  check_size(in, sizeof(T));
  std::memcpy(&obj, in.data(), sizeof(T));
  if constexpr (std::endian::native == std::endian::big && sizeof(T) > 1) {
    char *ptr = reinterpret_cast<char *>(&obj);
    std::reverse(ptr, ptr + sizeof(T));
  }
  in = in.subspan(sizeof(T));
}

inline void read_data(std::span<const char> &in, std::string &obj) {
  uint64_t size;
  read_data(in, size);
  check_size(in, size);
  obj.assign(in.data(), size);
  in = in.subspan(size);
}

template <typename T>
void read_data(std::span<const char> &in, std::vector<T> &obj) {
  uint64_t size;
  read_data(in, size);
  obj.resize(size);
  if constexpr (std::is_trivially_copyable_v<T> &&
                std::endian::native == std::endian::little) {
    check_size(in, size * sizeof(T));
    std::memcpy(obj.data(), in.data(), size * sizeof(T));
    in = in.subspan(size * sizeof(T));
  } else {
    for (auto &item : obj)
      read_data(in, item);
  }
}

template <typename T, size_t N>
void read_data(std::span<const char> &in, std::array<T, N> &obj) {
  for (auto &item : obj)
    read_data(in, item);
}

template <typename T>
void read_data(std::span<const char> &in, std::set<T> &obj) {
  uint64_t size;
  read_data(in, size);
  obj.clear();
  for (uint64_t i = 0; i < size; ++i) {
    T item;
    read_data(in, item);
    obj.insert(std::move(item));
  }
}

template <typename K, typename V>
void read_data(std::span<const char> &in, std::map<K, V> &obj) {
  uint64_t size;
  read_data(in, size);
  obj.clear();
  for (uint64_t i = 0; i < size; ++i) {
    K key;
    V val;
    read_data(in, key);
    read_data(in, val);
    obj.emplace(std::move(key), std::move(val));
  }
}

template <typename Tuple, std::size_t... I>
void read_tuple_impl(std::span<const char> &in, Tuple &obj,
                     std::index_sequence<I...>) {
  (read_data(in, std::get<I>(obj)), ...);
}

template <typename Tuple>
  requires tuple_like<Tuple>
void read_data(std::span<const char> &in, Tuple &obj) {
  constexpr std::size_t N = std::tuple_size_v<std::remove_cvref_t<Tuple>>;
  read_tuple_impl(in, obj, std::make_index_sequence<N>{});
}

template <typename Variant, std::size_t... I>
void read_variant_impl(std::span<const char> &in, uint64_t index, Variant &obj,
                       std::index_sequence<I...>) {
  ((index == I ? (void)(read_data(in, obj.template emplace<I>())) : (void)0),
   ...);
}

template <typename... Args>
void read_data(std::span<const char> &in, std::variant<Args...> &obj) {
  uint64_t index;
  read_data(in, index);
  if (index >= sizeof...(Args))
    throw std::runtime_error("Invalid variant index");
  read_variant_impl(in, index, obj,
                    std::make_index_sequence<sizeof...(Args)>{});
}

template <typename T>
void deserialize_from_buffer(const void *in, uint64_t size, T &obj) {
  std::span<const char> s(reinterpret_cast<const char *>(in), size);
  read_data(s, obj);
}

#define SERIALIZABLE(...)                                                      \
  friend void write_data(std::vector<char> &out, const auto &obj);             \
  friend void read_data(std::span<const char> &in, auto &obj);                 \
  void serialize(std::vector<char> &out) const {                               \
    auto write_all = [&](const auto &...args) {                                \
      (write_data(out, args), ...);                                            \
    };                                                                         \
    write_all(__VA_ARGS__);                                                    \
  }                                                                            \
  void deserialize(std::span<const char> &in) {                                \
    auto read_all = [&](auto &...args) { (read_data(in, args), ...); };        \
    read_all(__VA_ARGS__);                                                     \
  }                                                                            \
  std::vector<char> serialize() const {                                        \
    std::vector<char> out;                                                     \
    this->serialize(out);                                                      \
    return out;                                                                \
  }
