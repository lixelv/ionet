#pragma once

#include "k25519.hpp"
#include <fstream>
#include <stdexcept>

static ed25519_t p = {k25519_t(0xC9562D608F25D51AULL, 0x692CC7609525A7B2ULL,
                               0xC0A4E231FDD6DC5CULL, 0x216936D3CD6E53FEULL),
                      k25519_t(0x6666666666666658ULL, 0x6666666666666666ULL,
                               0x6666666666666666ULL, 0x6666666666666666ULL)};

inline uint256_t random_big_uint() {
  uint256_t out{};

  std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
  if (!urandom) {
    throw std::runtime_error("Cannot open /dev/urandom");
  }

  urandom.read(reinterpret_cast<char *>(&out), sizeof(out));

  if (!urandom) {
    throw std::runtime_error("Failed to read entropy");
  }

  return out;
}

inline std::pair<ed25519_t, uint256_t> generate_key_pair() {
  uint256_t priv = random_big_uint();
  ed25519_t pub = p * priv;
  return {pub, priv};
}