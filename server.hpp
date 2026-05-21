#pragma once

#include "session.hpp"
#include <arpa/inet.h>
#include <functional>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

struct Server {
  int server_fd = -1;
  uint16_t port;

  explicit Server(uint16_t port) : port(port) {
    server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
      throw std::runtime_error("socket() failed");
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) <
        0) {
      throw std::runtime_error("bind() failed");
    }

    if (::listen(server_fd, 16) < 0) {
      throw std::runtime_error("listen() failed");
    }
  }

  ~Server() {
    if (server_fd >= 0) {
      close(server_fd);
    }
  }

  CommonSession accept_one() {
    CommonSession sess;
    int fd = ::accept(server_fd, nullptr, nullptr);
    if (fd < 0) {
      return sess;
    }
    sess.set_fd(fd);

    if (!sess.handshake_responder()) {
      close(fd);
      sess.set_fd(-1);
    }
    return sess;
  }

  void listen_loop(std::function<void(CommonSession)> handler) {
    while (true) {
      CommonSession sess;
      try {
        sess = accept_one();
      } catch (const std::exception &e) {
        std::cerr << "accept error: " << e.what() << "\n";
        continue;
      } catch (...) {
        std::cerr << "accept error: unknown exception\n";
        continue;
      }

      if (sess.get_fd() < 0) {
        continue;
      }

      std::thread([h = handler, s = std::move(sess)]() mutable {
        h(std::move(s));
      }).detach();
    }
  }
};
