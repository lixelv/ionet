#pragma once

#include "serialize.hpp"
#include "socket.hpp"
#include <vector>

template <typename T>
bool send_struct(int fd, const T &obj, std::vector<char> &serialized) {
  write_data(serialized, obj);
  return send_bytes(fd, serialized);
}

template <typename T> bool send_struct(int fd, const T &obj) {
  std::vector<char> serialized;
  return send_struct(fd, obj, serialized);
}

template <typename T>
bool recv_struct(int fd, T &obj, std::vector<char> &serialized) {
  if (!recv_bytes(fd, serialized)) {
    return false;
  }

  std::span<const char> span(serialized.data(), serialized.size());
  try {
    read_data(span, obj);
  } catch (...) {
    return false;
  }

  return true;
}

template <typename T> bool recv_struct(int fd, T &obj) {
  std::vector<char> serialized;
  return recv_struct(fd, obj, serialized);
}
