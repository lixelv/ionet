#pragma once

#include "k25519.hpp"
#include <cstdint>
#include <cstring>

static inline uint32_t rotl(uint32_t x, int n) {
  return (x << n) | (x >> (32 - n));
}

static inline void quarter_round(uint32_t &a, uint32_t &b, uint32_t &c,
                                 uint32_t &d) {
  a += b;
  d ^= a;
  d = rotl(d, 16);
  c += d;
  b ^= c;
  b = rotl(b, 12);
  a += b;
  d ^= a;
  d = rotl(d, 8);
  c += d;
  b ^= c;
  b = rotl(b, 7);
}

struct ChaCha20 {
  uint32_t state[16];

  ChaCha20() = default;
  ChaCha20(const ChaCha20 &) = default;
  ChaCha20(ChaCha20 &&) = default;

  ChaCha20(const uint256_t &key, const uint8_t nonce[12],
           uint32_t counter = 0) {
    static const char constants[17] = "expand 32-byte k";

    state[0] = *(uint32_t *)(constants + 0);
    state[1] = *(uint32_t *)(constants + 4);
    state[2] = *(uint32_t *)(constants + 8);
    state[3] = *(uint32_t *)(constants + 12);

    for (int i = 0; i < 4; i++) {
      state[4 + i * 2] = (uint32_t)(key.limbs[i]);
      state[4 + i * 2 + 1] = (uint32_t)(key.limbs[i] >> 32);
    }

    state[12] = counter;
    state[13] = ((uint32_t *)nonce)[0];
    state[14] = ((uint32_t *)nonce)[1];
    state[15] = ((uint32_t *)nonce)[2];
  }

  ChaCha20 &operator=(const ChaCha20 &) = default;
  ChaCha20 &operator=(ChaCha20 &&) = default;
  ~ChaCha20() = default;

  void block(uint8_t output[64]) {
    uint32_t x[16];
    memcpy(x, state, sizeof(state));

    for (int i = 0; i < 10; i++) {
      quarter_round(x[0], x[4], x[8], x[12]);
      quarter_round(x[1], x[5], x[9], x[13]);
      quarter_round(x[2], x[6], x[10], x[14]);
      quarter_round(x[3], x[7], x[11], x[15]);

      quarter_round(x[0], x[5], x[10], x[15]);
      quarter_round(x[1], x[6], x[11], x[12]);
      quarter_round(x[2], x[7], x[8], x[13]);
      quarter_round(x[3], x[4], x[9], x[14]);
    }

    for (int i = 0; i < 16; i++) {
      x[i] += state[i];
    }

    memcpy(output, x, 64);
    state[12]++;
  }

  void crypt(uint8_t *data, size_t len) {
    uint8_t keystream[64];

    for (size_t i = 0; i < len; i += 64) {
      block(keystream);

      for (size_t j = 0; j < 64 && i + j < len; j++) {
        data[i + j] ^= keystream[j];
      }
    }
  }
};
