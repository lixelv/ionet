#include "ionet/session.hpp"
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <vector>

int g_pass = 0;
int g_fail = 0;

void Check(const char *name, bool ok) {
  if (ok) {
    std::cout << "[PASS] " << name << "\n";
    ++g_pass;
  } else {
    std::cerr << "[FAIL] " << name << "\n";
    ++g_fail;
  }
}

struct RoundTrip {
  std::string server_msg;
  uint64_t client_len = 0;
  bool ok = false;
};

RoundTrip RunCommon(bool encrypted) {
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
    return {};

  RoundTrip rt;

  std::thread server_thread([&] {
    CommonSession s;
    s.set_fd(sv[1]);
    if (!s.handshake_responder())
      return;

    std::string msg;
    if (!s.recv(msg))
      return;
    rt.server_msg = msg;
    s.send(static_cast<uint64_t>(msg.size()));
  });

  {
    CommonSession c;
    c.set_fd(sv[0]);
    SessionHeaders shi{encrypted};
    if (c.handshake_initiator(shi)) {
      std::string payload =
          encrypted ? "hello encrypted world" : "hello plain world";
      if (c.send(payload)) {
        uint64_t len = 0;
        if (c.recv(len)) {
          rt.client_len = len;
          rt.ok = true;
        }
      }
    }
  }

  server_thread.join();
  return rt;
}

void TestCommonEncrypted() {
  auto rt = RunCommon(/*encrypted=*/true);
  Check("common encrypted: handshake + round-trip ok", rt.ok);
  Check("common encrypted: server received correct message",
        rt.server_msg == "hello encrypted world");
  Check("common encrypted: client received correct length",
        rt.client_len == 21);
}

void TestCommonPlain() {
  auto rt = RunCommon(/*encrypted=*/false);
  Check("common plain: handshake + round-trip ok", rt.ok);
  Check("common plain: server received correct message",
        rt.server_msg == "hello plain world");
  Check("common plain: client received correct length", rt.client_len == 17);
}

void TestEncryptedSessionDirect() {
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
    ++g_fail;
    return;
  }

  std::string received;
  uint64_t received_len = 0;
  bool server_ok = false;

  std::thread server_thread([&] {
    EncryptedSession s;
    s.set_fd(sv[1]);
    if (!s.handshake_responder())
      return;
    std::string msg;
    if (!s.recv(msg))
      return;
    received = msg;
    server_ok = s.send(static_cast<uint64_t>(msg.size()));
  });

  EncryptedSession c;
  c.set_fd(sv[0]);
  bool client_ok = c.handshake_initiator() &&
                   c.send(std::string("hello encrypted world")) &&
                   c.recv(received_len);

  server_thread.join();

  Check("encrypted direct: handshake + send ok", client_ok && server_ok);
  Check("encrypted direct: server received correct message",
        received == "hello encrypted world");
  Check("encrypted direct: client received correct length", received_len == 21);
}

void TestPlainSessionDirect() {
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
    ++g_fail;
    return;
  }

  std::string received;
  uint64_t received_len = 0;
  bool server_ok = false;

  std::thread server_thread([&] {
    PlainSession s;
    s.set_fd(sv[1]);
    s.handshake_responder();
    std::string msg;
    if (!s.recv(msg))
      return;
    received = msg;
    server_ok = s.send(static_cast<uint64_t>(msg.size()));
  });

  PlainSession c;
  c.set_fd(sv[0]);
  bool client_ok = c.handshake_initiator() &&
                   c.send(std::string("hello plain world")) &&
                   c.recv(received_len);

  server_thread.join();

  Check("plain direct: handshake + send ok", client_ok && server_ok);
  Check("plain direct: server received correct message",
        received == "hello plain world");
  Check("plain direct: client received correct length", received_len == 17);
}

template <typename Client, typename Server>
void TestTypes(const char *label, Client &c, Server &s) {
  {
    std::string rx;
    bool ok = c.send(std::string("type test")) && s.recv(rx);
    Check((std::string(label) + ": send/recv string").c_str(),
          ok && rx == "type test");
  }
  {
    uint64_t rx = 0;
    bool ok = c.send(uint64_t{12345678}) && s.recv(rx);
    Check((std::string(label) + ": send/recv uint64").c_str(),
          ok && rx == 12345678);
  }
  {
    std::vector<std::string> rx;
    std::vector<std::string> tx{"foo", "bar", "baz"};
    bool ok = c.send(tx) && s.recv(rx);
    Check((std::string(label) + ": send/recv vector<string>").c_str(),
          ok && rx == tx);
  }
}

void TestTypesEncrypted() {
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
    ++g_fail;
    return;
  }

  EncryptedSession s, c;
  s.set_fd(sv[1]);
  c.set_fd(sv[0]);

  bool hs_ok = false;
  std::thread t([&] { hs_ok = s.handshake_responder(); });
  c.handshake_initiator();
  t.join();

  Check("encrypted types: handshake ok", hs_ok);
  auto run = [&](auto send_fn, auto recv_fn) -> bool {
    bool r = false;
    std::thread st([&] { r = recv_fn(); });
    bool s2 = send_fn();
    st.join();
    return s2 && r;
  };
  (void)run;
  {
    std::string rx;
    std::thread st([&] { s.recv(rx); });
    c.send(std::string("enc type test"));
    st.join();
    Check("encrypted types: string round-trip", rx == "enc type test");
  }
  {
    uint64_t rx = 0;
    std::thread st([&] { s.recv(rx); });
    c.send(uint64_t{99999});
    st.join();
    Check("encrypted types: uint64 round-trip", rx == 99999);
  }
  {
    std::vector<std::string> rx;
    std::vector<std::string> tx{"a", "bb", "ccc"};
    std::thread st([&] { s.recv(rx); });
    c.send(tx);
    st.join();
    Check("encrypted types: vector<string> round-trip", rx == tx);
  }
}

void TestTypesPlain() {
  int sv[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
    ++g_fail;
    return;
  }

  PlainSession s, c;
  s.set_fd(sv[1]);
  c.set_fd(sv[0]);
  s.handshake_responder();
  c.handshake_initiator();

  {
    std::string rx;
    std::thread st([&] { s.recv(rx); });
    c.send(std::string("plain type test"));
    st.join();
    Check("plain types: string round-trip", rx == "plain type test");
  }
  {
    uint64_t rx = 0;
    std::thread st([&] { s.recv(rx); });
    c.send(uint64_t{42});
    st.join();
    Check("plain types: uint64 round-trip", rx == 42);
  }
  {
    std::vector<std::string> rx;
    std::vector<std::string> tx{"x", "y", "z"};
    std::thread st([&] { s.recv(rx); });
    c.send(tx);
    st.join();
    Check("plain types: vector<string> round-trip", rx == tx);
  }
}

int main() {
  TestEncryptedSessionDirect();
  TestPlainSessionDirect();
  TestCommonEncrypted();
  TestCommonPlain();
  TestTypesEncrypted();
  TestTypesPlain();

  std::cout << "\nResults: " << g_pass << " passed, " << g_fail << " failed\n";
  return g_fail > 0 ? 1 : 0;
}
