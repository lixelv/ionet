#pragma once

#include "session.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

struct Client {
  CommonSession session;
  SessionHeaders shi;

  Client(const std::string &host, uint16_t port, bool encrypted = false)
      : shi(encrypted) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      throw std::runtime_error("socket() failed");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
      close(fd);
      throw std::runtime_error("inet_pton() failed: bad address " + host);
    }

    if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
      close(fd);
      throw std::runtime_error("connect() failed");
    }

    session.set_fd(fd);

    if (!session.handshake_initiator(shi)) {
      close(fd);
      session.set_fd(-1);
      throw std::runtime_error("ECDH handshake failed");
    }
  }

  ~Client() {
    if (session.get_fd() >= 0) {
      close(session.get_fd());
    }
  }

  template <typename T> bool send(const T &obj) { return session.send(obj); }
  template <typename T> bool recv(T &obj) { return session.recv(obj); }
};
