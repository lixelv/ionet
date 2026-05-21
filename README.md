# ionet

A header-only C++ networking library for building TCP applications with optional ECDH-negotiated ChaCha20 encryption. Zero external dependencies — only POSIX sockets.

## Features

- **Header-only** — drop the headers into your project and `#include`
- **Binary serialization** — `SERIALIZABLE` macro for custom structs; built-in support for primitives, `string`, `vector`, `array`, `set`, `map`, `pair`, `tuple`, `variant`
- **Plain or encrypted sessions** — mode negotiated at handshake time; no configuration needed on the server
- **ECDH key exchange** — custom Ed25519 scalar multiplication; no OpenSSL required
- **ChaCha20 stream cipher** — symmetric encryption after key agreement
- **Thread-safe accept loop** — `Server::listen_loop` dispatches each connection to a detached thread

## Requirements

| Component | Minimum |
|-----------|---------|
| C++ standard | C++23 |
| CMake | 3.20 |
| OS | Linux, macOS (POSIX sockets) |
| Compiler | GCC 13+, Clang 16+ |

## Project Structure

```
ionet/
├── client.hpp          # Client — connect + ECDH handshake
├── server.hpp          # Server — listen + accept
├── session.hpp         # PlainSession, EncryptedSession, CommonSession
├── connect/
│   ├── socket.hpp      # send_exactly / recv_exactly / send_bytes / recv_bytes
│   ├── serialize.hpp   # write_data / read_data + SERIALIZABLE macro
│   └── struct.hpp      # send_struct / recv_struct helpers
├── crypto/
│   ├── k25519.hpp      # GF(2²⁵⁵−19) arithmetic + Ed25519 point math
│   ├── chacha20.hpp    # ChaCha20 stream cipher
│   └── crypto.hpp      # Key pair generation via /dev/urandom
├── example/
│   ├── server.cpp      # Echo server example
│   └── client.cpp      # Interactive client example
└── tests/
    ├── ionet_test.cpp  # Serialization unit tests
    ├── session_test.cpp# Session / handshake unit tests
    ├── crypto_test.cpp # Crypto primitive unit tests
    └── system_test.cpp # End-to-end TCP integration tests
```

## Quick Start

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Run tests

```bash
ctest --test-dir build --output-on-failure
```

### Build examples

```bash
cmake -B build -DBUILD_EXAMPLES=ON
cmake --build build --parallel
```

Then in two terminals:

```bash
./build/example_server   # listens on port 8002 (plain mode)
./build/example_client   # connects and sends lines from stdin
```

## API Reference

### SERIALIZABLE macro

Makes any struct serializable over the wire:

```cpp
struct Point {
    float x, y;
    SERIALIZABLE(x, y)
};

struct Packet {
    std::string name;
    std::vector<Point> pts;
    uint32_t flags;
    SERIALIZABLE(name, pts, flags)
};
```

Fields listed in `SERIALIZABLE(...)` are written/read in order. Nested serializable types work automatically.

### Supported types (built-in)

| Type | Notes |
|------|-------|
| All arithmetic types | `bool`, `int8_t`…`int64_t`, `uint8_t`…`uint64_t`, `float`, `double` |
| `std::string` | Length-prefixed (uint64) |
| `std::vector<T>` | Length-prefixed; bulk copy for trivial types on little-endian |
| `std::array<T, N>` | Fixed size, no length prefix |
| `std::set<T>` | Length-prefixed |
| `std::map<K, V>` | Length-prefixed |
| `std::pair<A, B>` | Treated as 2-tuple |
| `std::tuple<...>` | Members in order |
| `std::variant<...>` | Index (uint64) then active member |
| Custom structs | Via `SERIALIZABLE(...)` |

### Server

```cpp
#include "server.hpp"

// Bind and listen immediately in the constructor
Server srv(8080);

// Accept a single connection (blocking)
CommonSession sess = srv.accept_one();
if (sess.get_fd() >= 0) {
    std::string msg;
    sess.recv(msg);
    sess.send(uint64_t{msg.size()});
    close(sess.get_fd());
}

// Blocking accept loop — each connection runs in a detached thread
srv.listen_loop([](CommonSession sess) {
    std::string msg;
    while (sess.recv(msg)) {
        sess.send(uint64_t{msg.size()});
    }
    close(sess.get_fd());
});
```

### Client

```cpp
#include "client.hpp"

// Plain session (no encryption)
Client plain("127.0.0.1", 8080, false);

// Encrypted session (ECDH + ChaCha20)
Client enc("127.0.0.1", 8080, true);

// Send / receive any serializable type
plain.send(std::string("hello"));
uint64_t len;
plain.recv(len);

struct Packet { int id; std::string body; SERIALIZABLE(id, body) };
enc.send(Packet{1, "secret"});
```

The `Client` constructor throws `std::runtime_error` on connection failure or handshake failure.

### Session types

| Type | Description |
|------|-------------|
| `PlainSession` | Raw framed I/O, no encryption |
| `EncryptedSession` | ECDH handshake → ChaCha20 in each direction |
| `CommonSession` | `std::variant`-based; mode negotiated at runtime via `SessionHeaders` |

`CommonSession` is what `Server::accept_one()` returns. The client's `encrypted` flag controls which mode is negotiated.

### Wire format

All messages use a 4-byte little-endian length prefix followed by the serialized payload. The handshake for an encrypted session exchanges Ed25519 public keys, then a 12-byte nonce (sent by the initiator). Counter values are split per-direction so initiator→responder and responder→initiator use independent keystreams.

## Cryptography

- **Elliptic curve**: Twisted Edwards curve over GF(2²⁵⁵−19) (Ed25519 base group)
- **Key exchange**: Ephemeral ECDH — new key pair per connection, shared secret never reused
- **Cipher**: ChaCha20 with counter 0x00000001 (initiator→responder) and 0x80000001 (responder→initiator)
- **Entropy**: `/dev/urandom`
- **No authentication**: The handshake provides confidentiality but not server identity verification. Add a higher-level auth layer if needed.

## Limitations

- POSIX only (Linux, macOS); no Windows support
- `connect/socket.hpp` defines functions without `inline`; include it in at most one translation unit per executable (header-only single-TU usage is the intended pattern)
- No TLS certificate / PKI — suitable for trusted networks or as a building block
- `listen_loop` has no graceful shutdown mechanism; close `server_fd` from another thread to unblock `accept()`

## License

MIT
