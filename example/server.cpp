#include "ionet/server.hpp"
#include <iostream>
#include <string>

int main() {
  Server srv(8002);
  std::cout << "Server listening on port 8002...\n";

  srv.listen_loop([](CommonSession sess) {
    std::string msg;

    while (sess.recv(msg)) {
      std::cout << "[server] received " << msg.size() << " bytes\n";
      sess.send(static_cast<uint64_t>(msg.size()));
    }
  });
}
