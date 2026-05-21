#include "../connect/struct.hpp"

#include <iostream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <variant>
#include <vector>

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
  std::variant<int, Point> a;
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
    return;
  }

  bool send_ok = false;
  std::thread sender([&]() { send_ok = send_struct(fds[0], sent); });

  T received{};
  bool recv_ok = recv_struct(fds[1], received);
  sender.join();

  if (!send_ok) {
    std::cerr << "[FAIL] " << name << ": send_struct returned false\n";
  } else if (!recv_ok) {
    std::cerr << "[FAIL] " << name << ": recv_struct returned false\n";
  } else {
    bool passed = (received == sent);
    std::cout << (passed ? "[PASS] " : "[FAIL] ") << name << "\n";
  }

  close(fds[0]);
  close(fds[1]);
}

int main() {
  run_test<Point>("Point (two floats)", {3.14f, 2.71f});

  run_test<Polyline>("Polyline (string + vector<Point>)",
                     {"zigzag", {{0.0f, 0.0f}, {1.0f, 2.0f}, {3.0f, -1.0f}}});

  run_test<Polyline>("Polyline with empty vector", {"empty", {}});

  run_test<Dataset>("Dataset (nested vectors of structs)",
                    {3,
                     "experiment-1",
                     {"alpha", "beta", "gamma"},
                     {{"line-A", {{1.0f, 2.0f}, {3.0f, 4.0f}}},
                      {"line-B", {{-1.0f, 0.0f}, {0.0f, -1.0f}, {1.0f, 0.0f}}}},
                     {100, -200, 300, 0, -1}});

  run_test<Dataset>("Dataset with all empty vectors",
                    {Point{3.14f, 2.71f}, "empty-dataset", {}, {}, {}});

  {
    Polyline big;
    big.name = "stress";
    for (int i = 0; i < 10000; ++i)
      big.points.push_back({static_cast<float>(i), static_cast<float>(-i)});
    run_test<Polyline>("Polyline with 10000 points (stress)", big);
  }

  return 0;
}
