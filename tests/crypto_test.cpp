#include "../crypto/chacha20.hpp"
#include "../crypto/crypto.hpp"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

int g_pass = 0;
int g_fail = 0;

void Check(const char *name, bool ok) {
  std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\n";
  ok ? ++g_pass : ++g_fail;
}

static bool u256_eq(const uint256_t &a, const uint256_t &b) {
  return a.limbs == b.limbs;
}

void TestK25519Identity() {
  k25519_t zero(0);
  k25519_t one(1);
  k25519_t x(42);

  Check("k25519: 0 + x == x", (zero + x) == x);
  Check("k25519: x + 0 == x", (x + zero) == x);
  Check("k25519: x - x == 0", (x - x) == zero);
  Check("k25519: 0 * x == 0", (zero * x) == zero);
  Check("k25519: 1 * x == x", (one * x) == x);
  Check("k25519: x * 1 == x", (x * one) == x);
}

void TestK25519Arithmetic() {
  k25519_t a(100), b(200), sum(300), d(50);

  Check("k25519: a + b == 300", (a + b) == sum);
  Check("k25519: sum - a == b", (sum - a) == b);
  Check("k25519: commutativity a+b == b+a", (a + b) == (b + a));
  Check("k25519: commutativity a*b == b*a", (a * b) == (b * a));
  Check("k25519: associativity (a+b)+d == a+(b+d)",
        ((a + b) + d) == (a + (b + d)));
  Check("k25519: distributivity a*(b+d) == a*b+a*d",
        (a * (b + d)) == (a * b + a * d));
}

void TestK25519Inversion() {
  k25519_t one(1);
  k25519_t x(7);
  k25519_t y(999999);

  Check("k25519: x * inv(x) == 1 (x=7)", (x * x.inv()) == one);
  Check("k25519: x * inv(x) == 1 (x=999999)", (y * y.inv()) == one);
  Check("k25519: inv(inv(x)) == x", (x.inv().inv()) == x);
}

void TestK25519Division() {
  k25519_t a(6), b(2), c(3);
  Check("k25519: 6/2 == 3", (a / b) == c);
  Check("k25519: (a/b)*b == a", ((a / b) * b) == a);
}

void TestEd25519ScalarZero() {
  uint256_t zero_scalar{};
  ed25519_t result = p * zero_scalar;

  Check("ed25519: P*0 gives neutral element x==0", result.x == k25519_t(0));
  Check("ed25519: P*0 gives neutral element y==1", result.y == k25519_t(1));
}

void TestEd25519ScalarOne() {
  uint256_t one_scalar(1, 0, 0, 0);
  ed25519_t result = p * one_scalar;

  Check("ed25519: P*1 == P (x)", result.x == p.x);
  Check("ed25519: P*1 == P (y)", result.y == p.y);
}

void TestEd25519DoubleConsistency() {
  uint256_t one_scalar(1, 0, 0, 0);
  uint256_t two_scalar(2, 0, 0, 0);

  ed25519_t p1 = p * one_scalar;
  ed25519_t p2 = p * two_scalar;
  ed25519_t pp = p1 + p1;

  Check("ed25519: 2*P == P+P (x)", p2.x == pp.x);
  Check("ed25519: 2*P == P+P (y)", p2.y == pp.y);
}

void TestEd25519AdditionCommutativity() {
  uint256_t s1(3, 0, 0, 0);
  uint256_t s2(7, 0, 0, 0);

  ed25519_t A = p * s1;
  ed25519_t B = p * s2;

  ed25519_t AB = A + B;
  ed25519_t BA = B + A;

  Check("ed25519: A+B == B+A (x)", AB.x == BA.x);
  Check("ed25519: A+B == B+A (y)", AB.y == BA.y);
}

void TestECDH() {
  auto [pub_a, priv_a] = generate_key_pair();
  auto [pub_b, priv_b] = generate_key_pair();

  ed25519_t shared_a = pub_b * priv_a;
  ed25519_t shared_b = pub_a * priv_b;
  Check("ECDH: shared secrets match (x)", shared_a.x == shared_b.x);
  Check("ECDH: shared secrets match (y)", shared_a.y == shared_b.y);
}

void TestECDHMultiplePairs() {
  for (int i = 0; i < 3; ++i) {
    auto [pub_a, priv_a] = generate_key_pair();
    auto [pub_b, priv_b] = generate_key_pair();

    ed25519_t sa = pub_b * priv_a;
    ed25519_t sb = pub_a * priv_b;

    Check(("ECDH round " + std::to_string(i) + ": x matches").c_str(),
          sa.x == sb.x);
    Check(("ECDH round " + std::to_string(i) + ": y matches").c_str(),
          sa.y == sb.y);
  }
}

void TestKeyPairUniqueness() {
  auto [pub1, priv1] = generate_key_pair();
  auto [pub2, priv2] = generate_key_pair();

  Check("keygen: private keys differ", !u256_eq(priv1, priv2));
  Check("keygen: public keys differ", !(pub1 == pub2));
}

void TestChaCha20Roundtrip() {
  uint8_t nonce[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  uint256_t key(0x0102030405060708ULL, 0x090a0b0c0d0e0f10ULL,
                0x1112131415161718ULL, 0x191a1b1c1d1e1f20ULL);

  std::string original = "Hello, ChaCha20! Testing encrypt-then-decrypt.";
  std::vector<uint8_t> data(original.begin(), original.end());

  ChaCha20 enc(key, nonce, 0);
  enc.crypt(data.data(), data.size());

  std::string after_encrypt(data.begin(), data.end());
  Check("chacha20: encrypted != original", after_encrypt != original);

  ChaCha20 dec(key, nonce, 0);
  dec.crypt(data.data(), data.size());

  std::string decrypted(data.begin(), data.end());
  Check("chacha20: decrypt(encrypt(x)) == x", decrypted == original);
}

void TestChaCha20DifferentCounters() {
  uint8_t nonce[12] = {};
  uint256_t key(1, 2, 3, 4);

  uint8_t block0[64] = {}, block1[64] = {};

  ChaCha20 c0(key, nonce, 0);
  ChaCha20 c1(key, nonce, 1);
  c0.crypt(block0, 64);
  c1.crypt(block1, 64);

  Check("chacha20: counter 0 != counter 1",
        std::memcmp(block0, block1, 64) != 0);
}

void TestChaCha20DifferentKeys() {
  uint8_t nonce[12] = {};
  uint256_t key1(1, 2, 3, 4);
  uint256_t key2(5, 6, 7, 8);

  uint8_t data1[64] = {}, data2[64] = {};

  ChaCha20(key1, nonce, 0).crypt(data1, 64);
  ChaCha20(key2, nonce, 0).crypt(data2, 64);

  Check("chacha20: different keys produce different output",
        std::memcmp(data1, data2, 64) != 0);
}

void TestChaCha20DifferentNonces() {
  uint256_t key(0xdeadbeefULL, 0xcafebabeULL, 0, 0);
  uint8_t nonce1[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  uint8_t nonce2[12] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  uint8_t data1[64] = {}, data2[64] = {};

  ChaCha20(key, nonce1, 0).crypt(data1, 64);
  ChaCha20(key, nonce2, 0).crypt(data2, 64);

  Check("chacha20: different nonces produce different output",
        std::memcmp(data1, data2, 64) != 0);
}

void TestChaCha20LargeData() {
  uint8_t nonce[12] = {0xde, 0xad, 0xbe, 0xef, 0, 0, 0, 0, 0, 0, 0, 0};
  uint256_t key(0xdeadbeefULL, 0xcafebabeULL, 0x12345678ULL, 0xabcdef01ULL);

  std::vector<uint8_t> data(1024 * 64, 0x42);
  std::vector<uint8_t> original = data;

  ChaCha20 enc(key, nonce, 0);
  enc.crypt(data.data(), data.size());

  Check("chacha20: large data encrypted != original", data != original);

  ChaCha20 dec(key, nonce, 0);
  dec.crypt(data.data(), data.size());

  Check("chacha20: large data decrypt roundtrip", data == original);
}

void TestChaCha20EmptyData() {
  uint8_t nonce[12] = {};
  uint256_t key(1, 0, 0, 0);

  ChaCha20 c(key, nonce, 0);
  c.crypt(nullptr, 0);
  Check("chacha20: zero-length crypt does not crash", true);
}

void TestDirectionCounters() {
  uint8_t nonce[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  uint256_t key(0xaabbccddULL, 0, 0, 0);

  uint8_t i2r[64] = {}, r2i[64] = {};

  ChaCha20(key, nonce, 1).crypt(i2r, 64);
  ChaCha20(key, nonce, 0x80000001).crypt(r2i, 64);
  Check("direction counters: I2R != R2I keystreams",
        std::memcmp(i2r, r2i, 64) != 0);
}

int main() {
  TestK25519Identity();
  TestK25519Arithmetic();
  TestK25519Inversion();
  TestK25519Division();

  TestEd25519ScalarZero();
  TestEd25519ScalarOne();
  TestEd25519DoubleConsistency();
  TestEd25519AdditionCommutativity();

  TestECDH();
  TestECDHMultiplePairs();
  TestKeyPairUniqueness();

  TestChaCha20Roundtrip();
  TestChaCha20DifferentCounters();
  TestChaCha20DifferentKeys();
  TestChaCha20DifferentNonces();
  TestChaCha20LargeData();
  TestChaCha20EmptyData();
  TestDirectionCounters();

  std::cout << "\nResults: " << g_pass << " passed, " << g_fail << " failed\n";
  return g_fail > 0 ? 1 : 0;
}
