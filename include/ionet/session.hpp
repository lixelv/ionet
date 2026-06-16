#pragma once

#include "connect/socket.hpp"
#include "connect/struct.hpp"
#include "crypto/chacha20.hpp"
#include "crypto/crypto.hpp"
#include <span>
#include <utility>
#include <variant>

static constexpr uint32_t COUNTER_I2R = 1;
static constexpr uint32_t COUNTER_R2I = 0x80000001;

template <typename T>
concept CSession = requires { typename T::is_session; };

struct SessionHeaders {
  bool encrypted = false;

  SERIALIZABLE(encrypted)

  SessionHeaders() = default;
  SessionHeaders(bool encrypted) : encrypted(encrypted) {};
  SessionHeaders(const SessionHeaders &) = default;
  SessionHeaders(SessionHeaders &&) = default;

  SessionHeaders &operator=(const SessionHeaders &) = default;
  SessionHeaders &operator=(SessionHeaders &&) = default;

  ~SessionHeaders() = default;
};

struct PlainSession {
  using is_session = void;

  int fd = -1;

  PlainSession() = default;
  PlainSession(int fd) : fd(fd) {};
  PlainSession(const PlainSession &) = default;
  PlainSession(PlainSession &&) = default;

  PlainSession &operator=(const PlainSession &) = default;
  PlainSession &operator=(PlainSession &&) = default;

  ~PlainSession() = default;

  void set_fd(int v) { this->fd = v; }
  int get_fd() const { return this->fd; }

  bool handshake_initiator(const SessionHeaders &shi = SessionHeaders{false}) {
    return true;
  }
  bool handshake_responder() { return true; };

  template <typename T> bool send(const T &obj) { return send_struct(fd, obj); }
  template <typename T> bool recv(T &obj) { return recv_struct(fd, obj); }
};

struct EncryptedSession {
  using is_session = void;

  int fd = -1;

  ChaCha20 tx;
  ChaCha20 rx;

  EncryptedSession() = default;
  EncryptedSession(int fd) : fd(fd) {};
  EncryptedSession(const EncryptedSession &) = default;
  EncryptedSession(EncryptedSession &&) = default;

  EncryptedSession &operator=(const EncryptedSession &) = default;
  EncryptedSession &operator=(EncryptedSession &&) = default;

  ~EncryptedSession() = default;

  void set_fd(int v) { this->fd = v; }
  int get_fd() const { return this->fd; }

  static uint256_t point_to_key(const ed25519_t &pt) { return pt.x.v; }

  bool handshake_initiator(const SessionHeaders &shi = SessionHeaders{true}) {
    auto [pub, priv] = generate_key_pair();

    if (!send_struct(fd, pub)) {
      return false;
    }

    ed25519_t peer_pub;
    if (!recv_struct(fd, peer_pub)) {
      return false;
    }

    ed25519_t shared_pt = peer_pub * priv;
    uint256_t key = point_to_key(shared_pt);

    uint8_t nonce[12];
    {
      uint256_t rnd = random_big_uint();
      for (int i = 0; i < 12; ++i) {
        nonce[i] = reinterpret_cast<uint8_t *>(&rnd)[i];
      }
    }

    if (!send_exactly(fd, nonce, 12)) {
      return false;
    }

    tx = ChaCha20(key, nonce, COUNTER_I2R);
    rx = ChaCha20(key, nonce, COUNTER_R2I);
    return true;
  }

  bool handshake_responder() {
    ed25519_t peer_pub;
    if (!recv_struct(fd, peer_pub)) {
      return false;
    }

    auto [pub, priv] = generate_key_pair();

    if (!send_struct(fd, pub)) {
      return false;
    }

    uint8_t nonce[12];
    if (!recv_exactly(fd, nonce, 12)) {
      return false;
    }

    ed25519_t shared_pt = peer_pub * priv;
    uint256_t key = point_to_key(shared_pt);

    tx = ChaCha20(key, nonce, COUNTER_R2I);
    rx = ChaCha20(key, nonce, COUNTER_I2R);
    return true;
  }

  template <typename T> bool send(const T &obj) {
    std::vector<char> buf;
    write_data(buf, obj);
    tx.crypt(reinterpret_cast<uint8_t *>(buf.data()), buf.size());
    return send_bytes(fd, buf);
  }

  template <typename T> bool recv(T &obj) {
    std::vector<char> buf;
    if (!recv_bytes(fd, buf)) {
      return false;
    }
    rx.crypt(reinterpret_cast<uint8_t *>(buf.data()), buf.size());
    std::span<const char> span(buf.data(), buf.size());
    try {
      read_data(span, obj);
    } catch (...) {
      return false;
    }
    return true;
  }
};

struct CommonSession {
  std::variant<PlainSession, EncryptedSession> session;

  CommonSession() = default;
  CommonSession(const CommonSession &) = default;
  CommonSession(CommonSession &&) = default;
  CommonSession(int fd) { set_fd(fd); };

  template <CSession T>
  explicit CommonSession(T &&s) : session(std::forward<T>(s)) {}

  CommonSession &operator=(const CommonSession &) = default;
  CommonSession &operator=(CommonSession &&) = default;

  ~CommonSession() = default;

  void set_fd(int v) {
    std::visit([&v](auto &s) { s.set_fd(v); }, session);
  };

  int get_fd() {
    return std::visit([](auto &s) { return s.fd; }, session);
  };

  bool handshake_initiator(const SessionHeaders &shi) {
    int fd = std::visit([](const auto &s) { return s.get_fd(); }, session);
    send(shi);

    if (shi.encrypted) {
      session.emplace<EncryptedSession>(fd);
    } // else session must stay plain

    return std::visit([&shi](auto &s) { return s.handshake_initiator(shi); },
                      session);
  }

  bool handshake_responder() {
    int fd = std::visit([](const auto &s) { return s.fd; }, session);
    SessionHeaders shi;
    if (!recv(shi))
      return false;

    if (shi.encrypted) {
      session.emplace<EncryptedSession>(fd);
    } // else session must stay plain

    return std::visit([](auto &s) { return s.handshake_responder(); }, session);
  }

  template <typename T> bool send(const T &obj) {
    return std::visit([&obj](auto &s) { return s.send(obj); }, session);
  }

  template <typename T> bool recv(T &obj) {
    return std::visit([&obj](auto &s) { return s.recv(obj); }, session);
  }
};