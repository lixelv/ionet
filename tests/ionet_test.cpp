#include "ionet/connect/struct.hpp"

#include <array>
#include <iostream>
#include <map>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <variant>
#include <vector>

int g_pass = 0;
int g_fail = 0;

struct Point {
  float x;
  float y;

  SERIALIZABLE(x, y)

  bool operator==(const Point &o) const { return x == o.x && y == o.y; }
};

struct Polyline {
  std::string name;
  std::vector<Point> points;

  SERIALIZABLE(name, points)

  bool operator==(const Polyline &o) const {
    return name == o.name && points == o.points;
  }
};

struct Dataset {
  std::string label;
  std::vector<std::string> tags;
  std::vector<Polyline> lines;
  std::vector<int32_t> scores;

  SERIALIZABLE(label, tags, lines, scores)

  bool operator==(const Dataset &o) const {
    return label == o.label && tags == o.tags && lines == o.lines &&
           scores == o.scores;
  }
};

template <typename T> void run_test(const std::string &name, const T &sent) {
  int fds[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
    std::cerr << "[FAIL] " << name << ": socketpair failed\n";
    ++g_fail;
    return;
  }

  bool send_ok = false;
  std::thread sender([&]() { send_ok = send_struct(fds[0], sent); });

  T received{};
  bool recv_ok = recv_struct(fds[1], received);
  sender.join();

  close(fds[0]);
  close(fds[1]);

  if (!send_ok) {
    std::cerr << "[FAIL] " << name << ": send_struct returned false\n";
    ++g_fail;
  } else if (!recv_ok) {
    std::cerr << "[FAIL] " << name << ": recv_struct returned false\n";
    ++g_fail;
  } else {
    bool passed = (received == sent);
    std::cout << (passed ? "[PASS] " : "[FAIL] ") << name << "\n";
    passed ? ++g_pass : ++g_fail;
  }
}

void check(const char *name, bool ok) {
  std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\n";
  ok ? ++g_pass : ++g_fail;
}

void TestBufferUnderrun() {
  std::vector<char> buf;
  write_data(buf, uint64_t{0xdeadbeef});

  buf.resize(4); // truncate: uint64 needs 8 bytes

  std::span<const char> sp(buf.data(), buf.size());
  uint64_t val = 0;
  bool threw = false;
  try {
    read_data(sp, val);
  } catch (const std::runtime_error &) {
    threw = true;
  }
  check("buffer underrun throws runtime_error", threw);
}

void TestVariantInvalidIndex() {
  std::vector<char> buf;
  write_data(buf, uint64_t{99}); // out-of-range index for variant<int, string>
  write_data(buf, int32_t{0});

  std::span<const char> sp(buf.data(), buf.size());
  std::variant<int32_t, std::string> v;
  bool threw = false;
  try {
    read_data(sp, v);
  } catch (const std::runtime_error &) {
    threw = true;
  }
  check("variant with invalid index throws", threw);
}

int main() {
  // --- original tests ---
  run_test<Point>("Point (two floats)", {3.14f, 2.71f});

  run_test<Polyline>("Polyline (string + vector<Point>)",
                     {"zigzag", {{0.0f, 0.0f}, {1.0f, 2.0f}, {3.0f, -1.0f}}});

  run_test<Polyline>("Polyline with empty vector", {"empty", {}});

  run_test<Dataset>("Dataset (nested vectors of structs)",
                    {"experiment-1",
                     {"alpha", "beta", "gamma"},
                     {{"line-A", {{1.0f, 2.0f}, {3.0f, 4.0f}}},
                      {"line-B", {{-1.0f, 0.0f}, {0.0f, -1.0f}, {1.0f, 0.0f}}}},
                     {100, -200, 300, 0, -1}});

  run_test<Dataset>("Dataset with all empty vectors",
                    {"empty-dataset", {}, {}, {}});

  {
    Polyline big;
    big.name = "stress";
    for (int i = 0; i < 10000; ++i)
      big.points.push_back({static_cast<float>(i), static_cast<float>(-i)});
    run_test<Polyline>("Polyline with 10000 points (stress)", big);
  }

  // --- arithmetic types ---
  run_test<bool>("bool true", true);
  run_test<bool>("bool false", false);
  run_test<uint8_t>("uint8_t max", 255u);
  run_test<int8_t>("int8_t min", -128);
  run_test<uint16_t>("uint16_t", uint16_t{65535});
  run_test<int16_t>("int16_t negative", int16_t{-1000});
  run_test<uint32_t>("uint32_t", uint32_t{0xdeadbeef});
  run_test<int32_t>("int32_t negative", int32_t{-123456});
  run_test<uint64_t>("uint64_t max", std::numeric_limits<uint64_t>::max());
  run_test<int64_t>("int64_t min", std::numeric_limits<int64_t>::min());
  run_test<float>("float negative", -3.14f);
  run_test<double>("double", 2.718281828459045);

  // --- string edge cases ---
  run_test<std::string>("empty string", std::string{});
  run_test<std::string>("string with spaces", std::string{"hello world"});
  run_test<std::string>("string with null bytes",
                        std::string("\x00\x01\x02", 3));

  // --- vector edge cases ---
  run_test<std::vector<int32_t>>("empty vector<int32_t>",
                                 std::vector<int32_t>{});
  run_test<std::vector<uint8_t>>("vector<uint8_t>",
                                 std::vector<uint8_t>{1, 2, 3, 255, 0});

  // --- array ---
  run_test<std::array<int32_t, 4>>("array<int32_t,4>",
                                   std::array<int32_t, 4>{10, -20, 30, -40});
  run_test<std::array<double, 3>>("array<double,3>",
                                  std::array<double, 3>{1.1, 2.2, 3.3});

  // --- set ---
  run_test<std::set<int32_t>>("set<int32_t>", std::set<int32_t>{1, 2, 3, 5, 8});
  run_test<std::set<int32_t>>("empty set<int32_t>", std::set<int32_t>{});
  run_test<std::set<std::string>>("set<string>",
                                  std::set<std::string>{"apple", "banana"});

  // --- map ---
  run_test<std::map<std::string, int32_t>>(
      "map<string,int32_t>",
      std::map<std::string, int32_t>{{"one", 1}, {"two", 2}, {"three", 3}});
  run_test<std::map<std::string, int32_t>>("empty map<string,int32_t>",
                                           std::map<std::string, int32_t>{});
  run_test<std::map<int32_t, std::string>>(
      "map<int32_t,string>",
      std::map<int32_t, std::string>{{1, "a"}, {2, "b"}, {-1, "neg"}});

  // nested map
  run_test<std::map<std::string, std::vector<int32_t>>>(
      "map<string, vector<int32_t>>",
      std::map<std::string, std::vector<int32_t>>{{"odds", {1, 3, 5}},
                                                   {"evens", {2, 4, 6}},
                                                   {"empty", {}}});

  // --- pair ---
  run_test<std::pair<int32_t, std::string>>(
      "pair<int32_t,string>", std::pair<int32_t, std::string>{42, "hello"});
  run_test<std::pair<double, bool>>("pair<double,bool>",
                                    std::pair<double, bool>{3.14, true});

  // --- tuple ---
  run_test<std::tuple<int32_t, float, std::string>>(
      "tuple<int32_t,float,string>",
      std::tuple<int32_t, float, std::string>{7, 2.5f, "world"});
  run_test<std::tuple<uint64_t, uint64_t>>(
      "tuple<uint64_t,uint64_t>",
      std::tuple<uint64_t, uint64_t>{0, std::numeric_limits<uint64_t>::max()});

  // --- variant ---
  run_test<std::variant<int32_t, std::string>>(
      "variant<int32_t,string> holding int",
      std::variant<int32_t, std::string>{int32_t{42}});
  run_test<std::variant<int32_t, std::string>>(
      "variant<int32_t,string> holding string",
      std::variant<int32_t, std::string>{std::string{"hello variant"}});

  // --- exception / error cases ---
  TestBufferUnderrun();
  TestVariantInvalidIndex();

  std::cout << "\nResults: " << g_pass << " passed, " << g_fail << " failed\n";
  return g_fail > 0 ? 1 : 0;
}
