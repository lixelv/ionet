#include "ionet/client.hpp"
#include "ionet/server.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

int g_pass = 0;
int g_fail = 0;

void Check(const char *name, bool ok) {
  std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\n";
  ok ? ++g_pass : ++g_fail;
}

static uint16_t get_port(const Server &srv) {
  sockaddr_in addr{};
  socklen_t len = sizeof(addr);
  getsockname(srv.server_fd, reinterpret_cast<sockaddr *>(&addr), &len);
  return ntohs(addr.sin_port);
}

struct Packet {
  std::string text;
  uint64_t id;
  std::vector<int32_t> values;

  SERIALIZABLE(text, id, values)

  bool operator==(const Packet &o) const {
    return text == o.text && id == o.id && values == o.values;
  }
};

void TestPlainEchoRoundTrip() {
  Server srv(0);
  uint16_t port = get_port(srv);

  std::string server_received;
  bool server_ok = false;

  std::thread server_thread([&] {
    CommonSession sess = srv.accept_one();
    if (sess.get_fd() < 0)
      return;
    std::string msg;
    if (!sess.recv(msg))
      return;
    server_received = msg;
    server_ok = sess.send(static_cast<uint64_t>(msg.size()));
    close(sess.get_fd());
  });

  uint64_t reply = 0;
  bool client_ok = false;
  try {
    Client client("127.0.0.1", port, false);
    bool s = client.send(std::string("hello plain system"));
    uint64_t len;
    bool r = client.recv(len);
    reply = len;
    client_ok = s && r;
  } catch (const std::exception &e) {
    std::cerr << "client error: " << e.what() << "\n";
  }

  server_thread.join();

  Check("plain echo: round trip ok", client_ok && server_ok);
  Check("plain echo: server received correct message",
        server_received == "hello plain system");
  Check("plain echo: client received correct length", reply == 18);
}

void TestEncryptedEchoRoundTrip() {
  Server srv(0);
  uint16_t port = get_port(srv);

  std::string server_received;
  bool server_ok = false;

  std::thread server_thread([&] {
    CommonSession sess = srv.accept_one();
    if (sess.get_fd() < 0)
      return;
    std::string msg;
    if (!sess.recv(msg))
      return;
    server_received = msg;
    server_ok = sess.send(static_cast<uint64_t>(msg.size()));
    close(sess.get_fd());
  });

  uint64_t reply = 0;
  bool client_ok = false;
  try {
    Client client("127.0.0.1", port, true);
    bool s = client.send(std::string("hello encrypted system"));
    uint64_t len;
    bool r = client.recv(len);
    reply = len;
    client_ok = s && r;
  } catch (const std::exception &e) {
    std::cerr << "client error: " << e.what() << "\n";
  }

  server_thread.join();

  Check("encrypted echo: round trip ok", client_ok && server_ok);
  Check("encrypted echo: server received correct message",
        server_received == "hello encrypted system");
  Check("encrypted echo: client received correct length", reply == 22);
}

void TestMultipleMessages() {
  Server srv(0);
  uint16_t port = get_port(srv);
  const int N = 10;

  std::vector<std::string> server_received;
  bool server_ok = false;

  std::thread server_thread([&] {
    CommonSession sess = srv.accept_one();
    if (sess.get_fd() < 0)
      return;
    for (int i = 0; i < N; ++i) {
      std::string msg;
      if (!sess.recv(msg))
        return;
      server_received.push_back(msg);
      if (!sess.send(static_cast<uint64_t>(msg.size())))
        return;
    }
    server_ok = true;
    close(sess.get_fd());
  });

  bool client_ok = true;
  try {
    Client client("127.0.0.1", port, false);
    for (int i = 0; i < N; ++i) {
      std::string msg = "message-" + std::to_string(i);
      if (!client.send(msg)) {
        client_ok = false;
        break;
      }
      uint64_t len;
      if (!client.recv(len)) {
        client_ok = false;
        break;
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "client error: " << e.what() << "\n";
    client_ok = false;
  }

  server_thread.join();

  Check("multi-msg: all messages exchanged", client_ok && server_ok);
  Check("multi-msg: correct count",
        static_cast<int>(server_received.size()) == N);
  for (int i = 0; i < N && i < static_cast<int>(server_received.size()); ++i) {
    std::string expected = "message-" + std::to_string(i);
    Check(("multi-msg: msg[" + std::to_string(i) + "]").c_str(),
          server_received[i] == expected);
  }
}

void TestLargePayload() {
  Server srv(0);
  uint16_t port = get_port(srv);

  const size_t SZ = 1024 * 512;
  std::vector<uint8_t> sent(SZ);
  for (size_t i = 0; i < SZ; ++i)
    sent[i] = static_cast<uint8_t>(i & 0xff);

  std::vector<uint8_t> server_received;
  bool server_ok = false;

  std::thread server_thread([&] {
    CommonSession sess = srv.accept_one();
    if (sess.get_fd() < 0)
      return;
    if (!sess.recv(server_received))
      return;
    server_ok = sess.send(static_cast<uint64_t>(server_received.size()));
    close(sess.get_fd());
  });

  uint64_t reply = 0;
  bool client_ok = false;
  try {
    Client client("127.0.0.1", port, false);
    bool s = client.send(sent);
    uint64_t len;
    bool r = client.recv(len);
    reply = len;
    client_ok = s && r;
  } catch (const std::exception &e) {
    std::cerr << "client error: " << e.what() << "\n";
  }

  server_thread.join();

  Check("large payload: transfer ok", client_ok && server_ok);
  Check("large payload: correct size reported", reply == SZ);
  Check("large payload: data integrity", server_received == sent);
}

void TestCustomStruct() {
  Server srv(0);
  uint16_t port = get_port(srv);

  Packet sent{"system test packet", 0xdeadbeef, {1, -2, 3, -4, 42}};
  Packet server_received;
  Packet client_received;
  bool server_ok = false;

  std::thread server_thread([&] {
    CommonSession sess = srv.accept_one();
    if (sess.get_fd() < 0)
      return;
    if (!sess.recv(server_received))
      return;
    server_ok = sess.send(server_received);
    close(sess.get_fd());
  });

  bool client_ok = false;
  try {
    Client client("127.0.0.1", port, false);
    bool s = client.send(sent);
    bool r = client.recv(client_received);
    client_ok = s && r;
  } catch (const std::exception &e) {
    std::cerr << "client error: " << e.what() << "\n";
  }

  server_thread.join();

  Check("custom struct: round trip ok", client_ok && server_ok);
  Check("custom struct: server received correctly", server_received == sent);
  Check("custom struct: client received echo", client_received == sent);
}

void TestEncryptedCustomStruct() {
  Server srv(0);
  uint16_t port = get_port(srv);

  Packet sent{"encrypted packet", 1234567890ULL, {10, 20, 30}};
  Packet server_received;
  bool server_ok = false;

  std::thread server_thread([&] {
    CommonSession sess = srv.accept_one();
    if (sess.get_fd() < 0)
      return;
    if (!sess.recv(server_received))
      return;
    server_ok = sess.send(static_cast<uint64_t>(server_received.values.size()));
    close(sess.get_fd());
  });

  uint64_t reply = 0;
  bool client_ok = false;
  try {
    Client client("127.0.0.1", port, true);
    bool s = client.send(sent);
    uint64_t len;
    bool r = client.recv(len);
    reply = len;
    client_ok = s && r;
  } catch (const std::exception &e) {
    std::cerr << "client error: " << e.what() << "\n";
  }

  server_thread.join();

  Check("enc custom struct: round trip ok", client_ok && server_ok);
  Check("enc custom struct: server received correctly",
        server_received == sent);
  Check("enc custom struct: client got correct values count", reply == 3);
}

void TestMultipleSequentialClients() {
  Server srv(0);
  uint16_t port = get_port(srv);
  const int N = 5;

  std::vector<std::string> server_received(N);
  std::atomic<int> ok_count{0};

  std::thread server_thread([&] {
    for (int i = 0; i < N; ++i) {
      CommonSession sess = srv.accept_one();
      if (sess.get_fd() < 0)
        continue;
      std::string msg;
      if (sess.recv(msg)) {
        server_received[i] = msg;
        sess.send(static_cast<uint64_t>(i));
        ++ok_count;
      }
      close(sess.get_fd());
    }
  });

  for (int i = 0; i < N; ++i) {
    try {
      Client client("127.0.0.1", port, false);
      client.send(std::string("client-") + std::to_string(i));
      uint64_t idx;
      client.recv(idx);
    } catch (const std::exception &e) {
      std::cerr << "client " << i << " error: " << e.what() << "\n";
    }
  }

  server_thread.join();

  Check("sequential clients: all handled", ok_count.load() == N);
  for (int i = 0; i < N; ++i) {
    std::string expected = "client-" + std::to_string(i);
    Check(("sequential clients: client[" + std::to_string(i) + "]").c_str(),
          server_received[i] == expected);
  }
}

void TestMixedTypes() {
  Server srv(0);
  uint16_t port = get_port(srv);

  bool server_ok = false;
  std::string rx_str;
  uint64_t rx_u64 = 0;
  std::vector<int32_t> rx_vec;

  std::thread server_thread([&] {
    CommonSession sess = srv.accept_one();
    if (sess.get_fd() < 0)
      return;
    if (!sess.recv(rx_str))
      return;
    if (!sess.recv(rx_u64))
      return;
    if (!sess.recv(rx_vec))
      return;
    server_ok = sess.send(static_cast<uint64_t>(rx_str.size() + rx_vec.size()));
    close(sess.get_fd());
  });

  uint64_t reply = 0;
  bool client_ok = false;
  try {
    Client client("127.0.0.1", port, false);
    bool a = client.send(std::string("hello"));
    bool b = client.send(uint64_t{42});
    bool c = client.send(std::vector<int32_t>{1, 2, 3});
    uint64_t len;
    bool d = client.recv(len);
    reply = len;
    client_ok = a && b && c && d;
  } catch (const std::exception &e) {
    std::cerr << "client error: " << e.what() << "\n";
  }

  server_thread.join();

  Check("mixed types: all transfers ok", client_ok && server_ok);
  Check("mixed types: string received", rx_str == "hello");
  Check("mixed types: uint64 received", rx_u64 == 42);
  Check("mixed types: vector received",
        (rx_vec == std::vector<int32_t>{1, 2, 3}));
  Check("mixed types: summary length correct", reply == 5 + 3);
}

void TestEncryptedLargePayload() {
  Server srv(0);
  uint16_t port = get_port(srv);

  const size_t SZ = 1024 * 256;
  std::vector<uint8_t> sent(SZ);
  for (size_t i = 0; i < SZ; ++i)
    sent[i] = static_cast<uint8_t>((i * 7 + 13) & 0xff);

  std::vector<uint8_t> server_received;
  bool server_ok = false;

  std::thread server_thread([&] {
    CommonSession sess = srv.accept_one();
    if (sess.get_fd() < 0)
      return;
    if (!sess.recv(server_received))
      return;
    server_ok = sess.send(static_cast<uint64_t>(server_received.size()));
    close(sess.get_fd());
  });

  uint64_t reply = 0;
  bool client_ok = false;
  try {
    Client client("127.0.0.1", port, true);
    bool s = client.send(sent);
    uint64_t len;
    bool r = client.recv(len);
    reply = len;
    client_ok = s && r;
  } catch (const std::exception &e) {
    std::cerr << "client error: " << e.what() << "\n";
  }

  server_thread.join();

  Check("enc large payload: transfer ok", client_ok && server_ok);
  Check("enc large payload: correct size", reply == SZ);
  Check("enc large payload: data integrity", server_received == sent);
}

int main() {
  TestPlainEchoRoundTrip();
  TestEncryptedEchoRoundTrip();
  TestMultipleMessages();
  TestLargePayload();
  TestCustomStruct();
  TestEncryptedCustomStruct();
  TestMultipleSequentialClients();
  TestMixedTypes();
  TestEncryptedLargePayload();

  std::cout << "\nResults: " << g_pass << " passed, " << g_fail << " failed\n";
  return g_fail > 0 ? 1 : 0;
}
