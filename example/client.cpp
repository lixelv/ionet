#include "ionet/client.hpp"
#include <iostream>
#include <string>

int main() {
  try {
    Client client("127.0.0.1", 8002);
    std::cout << "Connected (encrypted). Type messages, Ctrl+C to quit.\n";

    std::string input;
    while (std::getline(std::cin, input)) {
      if (input.empty())
        continue;

      if (!client.send(input)) {
        std::cerr << "send failed\n";
        break;
      }

      uint64_t reply;
      if (!client.recv(reply)) {
        std::cerr << "recv failed\n";
        break;
      }

      std::cout << "Server reply: " << reply << " bytes received\n";
    }
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
