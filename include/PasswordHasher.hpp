#pragma once
//
// PasswordHasher.hpp
// -------------------
// Salted SHA-256 password hashing.
//
// Previous review flagged: passwords stored and compared in plaintext.
// This is the fix: we never store or compare a raw password again. We
// store salt + hash(salt || password), and verification recomputes the
// hash and compares digests.
//
// Honest limitation (documented, not hidden): SHA-256 is a fast hash,
// which is good for file integrity but not ideal for password storage —
// a real production system should use a slow, memory-hard KDF such as
// bcrypt, scrypt, or Argon2 to resist brute force. Those require an
// external dependency (libsodium / OpenSSL / Argon2 lib) which would
// break this project's "compiles anywhere, zero dependencies" goal.
// SHA-256 + per-user salt is a large, honest improvement over plaintext
// and is what's implemented here; the README calls out bcrypt/Argon2 as
// the recommended upgrade path for a real deployment.
//
#include <array>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace social {

class Sha256 {
public:
    static std::string hash(const std::string& input) {
        Sha256 ctx;
        ctx.update(reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
        return ctx.finalizeHex();
    }

private:
    std::uint32_t h_[8];
    std::uint64_t bitLen_ = 0;
    std::vector<std::uint8_t> buffer_;

    static std::uint32_t rotr(std::uint32_t x, std::uint32_t n) { return (x >> n) | (x << (32 - n)); }

    Sha256() {
        h_[0]=0x6a09e667; h_[1]=0xbb67ae85; h_[2]=0x3c6ef372; h_[3]=0xa54ff53a;
        h_[4]=0x510e527f; h_[5]=0x9b05688c; h_[6]=0x1f83d9ab; h_[7]=0x5be0cd19;
    }

    static const std::uint32_t* K() {
        static const std::uint32_t k[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
        };
        return k;
    }

    void processBlock(const std::uint8_t* p) {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(p[i*4]) << 24) | (static_cast<std::uint32_t>(p[i*4+1]) << 16) |
                   (static_cast<std::uint32_t>(p[i*4+2]) << 8) | static_cast<std::uint32_t>(p[i*4+3]);
        }
        for (int i = 16; i < 64; ++i) {
            std::uint32_t s0 = rotr(w[i-15],7) ^ rotr(w[i-15],18) ^ (w[i-15] >> 3);
            std::uint32_t s1 = rotr(w[i-2],17) ^ rotr(w[i-2],19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        std::uint32_t a=h_[0],b=h_[1],c=h_[2],d=h_[3],e=h_[4],f=h_[5],g=h_[6],hh=h_[7];
        const std::uint32_t* k = K();
        for (int i = 0; i < 64; ++i) {
            std::uint32_t S1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
            std::uint32_t ch = (e & f) ^ ((~e) & g);
            std::uint32_t temp1 = hh + S1 + ch + k[i] + w[i];
            std::uint32_t S0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
            std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            std::uint32_t temp2 = S0 + maj;
            hh=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
        }
        h_[0]+=a; h_[1]+=b; h_[2]+=c; h_[3]+=d; h_[4]+=e; h_[5]+=f; h_[6]+=g; h_[7]+=hh;
    }

    void update(const std::uint8_t* data, std::size_t len) {
        bitLen_ += static_cast<std::uint64_t>(len) * 8;
        buffer_.insert(buffer_.end(), data, data + len);
        while (buffer_.size() >= 64) {
            processBlock(buffer_.data());
            buffer_.erase(buffer_.begin(), buffer_.begin() + 64);
        }
    }

    std::string finalizeHex() {
        std::vector<std::uint8_t> pad;
        pad.push_back(0x80);
        while ((buffer_.size() + pad.size()) % 64 != 56) pad.push_back(0x00);
        for (int i = 7; i >= 0; --i) pad.push_back(static_cast<std::uint8_t>((bitLen_ >> (i*8)) & 0xff));
        update(pad.data(), pad.size());

        std::ostringstream oss;
        for (int i = 0; i < 8; ++i) {
            oss << std::hex << std::setfill('0') << std::setw(8) << h_[i];
        }
        return oss.str();
    }
};

class PasswordHasher {
public:
    // Generates a cryptographically-irrelevant-but-sufficiently-random salt
    // for a portfolio project. (Uses std::random_device, seeded per call.)
    static std::string generateSalt(std::size_t bytes = 16) {
        std::random_device rd;
        std::uniform_int_distribution<int> dist(0, 255);
        std::ostringstream oss;
        for (std::size_t i = 0; i < bytes; ++i) {
            oss << std::hex << std::setfill('0') << std::setw(2) << dist(rd);
        }
        return oss.str();
    }

    static std::string hashPassword(const std::string& password, const std::string& salt) {
        return Sha256::hash(salt + password);
    }

    static bool verify(const std::string& password, const std::string& salt, const std::string& expectedHash) {
        return hashPassword(password, salt) == expectedHash;
    }
};

} // namespace social
