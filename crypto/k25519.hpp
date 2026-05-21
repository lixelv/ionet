#pragma once

#include "../connect/serialize.hpp"
#include <array>
#include <cstdint>
#include <iostream>

struct uint256_t {
  std::array<uint64_t, 4> limbs{};

  uint256_t() {}

  uint256_t(uint64_t x, uint64_t y, uint64_t z, uint64_t w) {
    limbs[0] = x;
    limbs[1] = y;
    limbs[2] = z;
    limbs[3] = w;
  }

  SERIALIZABLE(limbs);

  uint256_t &operator=(const uint256_t &lhs) {
    limbs = lhs.limbs;
    return *this;
  }

  uint256_t from_hex(std::string) {
    uint256_t r;
    return r;
  }

  uint256_t operator^(const uint256_t &lhs) {
    uint256_t r;

    for (int i = 0; i < 4; i++) {

      r.limbs[i] = this->limbs[i] ^ lhs.limbs[i];
    }

    return r;
  }

  inline uint256_t operator+(const uint256_t &lhs) {
    uint256_t r;
    __uint128_t carry = 0;

    for (int i = 0; i < 4; i++) {
      __uint128_t sum = (__uint128_t)this->limbs[i] + lhs.limbs[i] + carry;
      r.limbs[i] = (uint64_t)sum;
      carry = sum >> 64;
    }

    return r;
  }

  inline uint256_t operator>>(int shift) {
    uint256_t r{};

    if (shift <= 0) {
      return *this;
    }
    if (shift >= 256) {
      return r;
    }

    int limb_shift = shift / 64;
    int bit_shift = shift % 64;

    for (int i = 0; i < 4; i++) {
      int src = i + limb_shift;
      if (src >= 4) {
        r.limbs[i] = 0;
        continue;
      }

      uint64_t low = this->limbs[src] >> bit_shift;
      uint64_t high = 0;

      if (bit_shift != 0 && src + 1 < 4) {
        high = this->limbs[src + 1] << (64 - bit_shift);
      }

      r.limbs[i] = low | high;
    }

    return r;
  }
};

struct k25519_t {
  uint256_t v;

  SERIALIZABLE(v);

  static constexpr uint64_t P[4] = {
      0xFFFFFFFFFFFFFFEDULL,
      0xFFFFFFFFFFFFFFFFULL,
      0xFFFFFFFFFFFFFFFFULL,
      0x7FFFFFFFFFFFFFFFULL,
  };

  k25519_t() = default;

  explicit k25519_t(uint64_t x) {
    v.limbs[0] = x;
    v.limbs[1] = v.limbs[2] = v.limbs[3] = 0;
    reduce_once();
  }

  explicit k25519_t(uint64_t x, uint64_t y, uint64_t z, uint64_t w) {
    v.limbs[0] = x;
    v.limbs[1] = y;
    v.limbs[2] = z;
    v.limbs[3] = w;
    reduce_once();
  }

  k25519_t(int x) : v() {
    if (x >= 0) {
      v.limbs[0] = (uint64_t)x;
      v.limbs[1] = v.limbs[2] = v.limbs[3] = 0;
    } else {
      uint64_t mag = (uint64_t)(-(int64_t)x);
      uint64_t borrow = 0;
      for (int i = 0; i < 4; ++i) {
        __uint128_t sub = (uint64_t)(i == 0 ? mag : 0);
        __uint128_t d = (__uint128_t)P[i] - sub - borrow;
        v.limbs[i] = (uint64_t)d;
        borrow = (d >> 127) & 1;
      }
    }
  }

  k25519_t &operator=(uint64_t x) {
    v.limbs[0] = x;
    v.limbs[1] = v.limbs[2] = v.limbs[3] = 0;
    reduce_once();
    return *this;
  }

  bool operator==(const k25519_t &o) const {
    return v.limbs[0] == o.v.limbs[0] && v.limbs[1] == o.v.limbs[1] &&
           v.limbs[2] == o.v.limbs[2] && v.limbs[3] == o.v.limbs[3];
  }
  bool operator!=(const k25519_t &o) const { return !(*this == o); }
  bool operator==(int o) const { return *this == k25519_t(o); }
  bool operator!=(int o) const { return !(*this == o); }

  k25519_t operator+(const k25519_t &o) const {
    k25519_t r;
    uint64_t carry = 0;
    for (int i = 0; i < 4; ++i) {
      __uint128_t s = (__uint128_t)v.limbs[i] + o.v.limbs[i] + carry;
      r.v.limbs[i] = (uint64_t)s;
      carry = (uint64_t)(s >> 64);
    }
    r.reduce_once();
    return r;
  }

  k25519_t operator-(const k25519_t &o) const {
    uint64_t tmp[4];
    uint64_t borrow = 0;
    for (int i = 0; i < 4; ++i) {
      __uint128_t d = (__uint128_t)v.limbs[i] - o.v.limbs[i] - borrow;
      tmp[i] = (uint64_t)d;
      borrow = (d >> 127) & 1;
    }

    uint64_t carry = 0;
    k25519_t r;
    for (int i = 0; i < 4; ++i) {
      __uint128_t s = (__uint128_t)tmp[i] + P[i] * borrow + carry;
      r.v.limbs[i] = (uint64_t)s;
      carry = (uint64_t)(s >> 64);
    }
    r.reduce_once();
    return r;
  }

  k25519_t operator*(const k25519_t &o) const {
    uint64_t w[8]{};
    for (int i = 0; i < 4; ++i) {
      __uint128_t carry = 0;
      for (int j = 0; j < 4; ++j) {
        __uint128_t t =
            (__uint128_t)v.limbs[i] * o.v.limbs[j] + w[i + j] + carry;
        w[i + j] = (uint64_t)t;
        carry = t >> 64;
      }
      w[i + 4] += (uint64_t)carry;
    }

    uint64_t r[4]{};
    __uint128_t carry = 0;
    for (int i = 0; i < 4; ++i) {
      __uint128_t s = (__uint128_t)w[i] + (__uint128_t)w[i + 4] * 38 + carry;
      r[i] = (uint64_t)s;
      carry = s >> 64;
    }

    {
      __uint128_t s = (__uint128_t)r[0] + carry * 38;
      r[0] = (uint64_t)s;
      carry = s >> 64;
      for (int i = 1; i < 4 && carry; ++i) {
        __uint128_t t = (__uint128_t)r[i] + carry;
        r[i] = (uint64_t)t;
        carry = t >> 64;
      }
    }

    k25519_t res;
    for (int i = 0; i < 4; ++i)
      res.v.limbs[i] = r[i];
    res.reduce_once();
    res.reduce_once();
    return res;
  }

  k25519_t pow(const uint64_t exp[4]) const {
    k25519_t result(1);
    k25519_t base(*this);
    for (int w = 0; w < 4; ++w) {
      uint64_t word = exp[w];
      for (int b = 0; b < 64; ++b, word >>= 1) {
        if (word & 1ULL)
          result = result * base;
        base = base * base;
      }
    }
    return result;
  }

  k25519_t inv() const {
    uint64_t exp[4] = {
        0xFFFFFFFFFFFFFFEBULL,
        0xFFFFFFFFFFFFFFFFULL,
        0xFFFFFFFFFFFFFFFFULL,
        0x7FFFFFFFFFFFFFFFULL,
    };
    return pow(exp);
  }

  k25519_t operator/(const k25519_t &other) const {
    return *this * other.inv();
  }

  friend std::ostream &operator<<(std::ostream &os, const k25519_t &v) {
    bool leading = true;
    for (int w = 3; w >= 0; --w) {
      uint64_t word = v.v.limbs[w];
      for (int shift = 60; shift >= 0; shift -= 4) {
        int nibble = (word >> shift) & 0xF;
        if (leading && nibble == 0)
          continue;
        leading = false;
        os << "0123456789abcdef"[nibble];
      }
    }
    if (leading)
      os << '0';
    return os;
  }

private:
  void reduce_once() {
    int cmp = 0;
    for (int i = 3; i >= 0; --i) {
      if (v.limbs[i] > P[i]) {
        cmp = 1;
        break;
      }
      if (v.limbs[i] < P[i]) {
        cmp = -1;
        break;
      }
    }

    if (cmp < 0) {
      return;
    }

    uint64_t borrow = 0;
    for (int i = 0; i < 4; ++i) {
      __uint128_t d = (__uint128_t)v.limbs[i] - P[i] - borrow;
      v.limbs[i] = (uint64_t)d;
      borrow = (d >> 127) & 1;
    }
  }
};

static const k25519_t D = [] {
  k25519_t v;
  v.v.limbs[0] = 0x75eb4dca135978a3ULL;
  v.v.limbs[1] = 0x00700a4d4141d8abULL;
  v.v.limbs[2] = 0x8cc740797779e898ULL;
  v.v.limbs[3] = 0x52036cee2b6ffe73ULL;
  return v;
}();

struct ed25519_t {
  k25519_t x;
  k25519_t y;

  SERIALIZABLE(x, y);

  ed25519_t() : x(0), y(1) {}
  ed25519_t(k25519_t x, k25519_t y) : x(x), y(y) {}

  ed25519_t &operator=(const ed25519_t &o) {
    x = o.x;
    y = o.y;
    return *this;
  }

  bool operator==(const ed25519_t &o) const { return x == o.x && y == o.y; }

  ed25519_t operator+(const ed25519_t &o) const {
    k25519_t dxy = D * x * o.x * y * o.y;
    return ed25519_t((x * o.y + y * o.x) / (k25519_t(1) + dxy),
                     (y * o.y + x * o.x) / (k25519_t(1) - dxy));
  }

  ed25519_t operator*(uint256_t scalar) const {
    ed25519_t r = {0, 1};
    ed25519_t cur = *this;

    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 64; ++j) {
        if (scalar.limbs[i] & (1ULL << j))
          r = r + cur;
        cur = cur + cur;
      }
    }

    return r;
  }
};