#pragma once

#include <cstdint>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

bool send_exactly(int fd, const void *data, size_t len) {
  const char *ptr = reinterpret_cast<const char *>(data);
  while (len > 0) {
    ssize_t sent = send(fd, ptr, len, 0);
    if (sent <= 0)
      return false;
    ptr += sent;
    len -= sent;
  }
  return true;
}

bool recv_exactly(int fd, void *data, size_t len) {
  char *ptr = reinterpret_cast<char *>(data);
  while (len > 0) {
    ssize_t received = recv(fd, ptr, len, 0);
    if (received <= 0)
      return false;
    ptr += received;
    len -= received;
  }
  return true;
}

bool send_bytes(int fd, const void *data, size_t len) {
  uint32_t size = static_cast<uint32_t>(len);

  if (!send_exactly(fd, &size, sizeof(size))) {
    return false;
  }

  return send_exactly(fd, data, len);
}

bool recv_bytes(int fd, std::vector<char> &buffer) {
  uint32_t size = 0;

  if (!recv_exactly(fd, &size, sizeof(size))) {
    return false;
  }

  buffer.resize(size);
  if (size > 0) {
    if (!recv_exactly(fd, buffer.data(), size)) {
      return false;
    }
  }

  return true;
}

bool send_bytes(int fd, const std::vector<char> &data) {
  return send_bytes(fd, data.data(), data.size());
}