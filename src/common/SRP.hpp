#pragma once

#include <openssl/bn.h>
#include <openssl/sha.h>
#include <openssl/rand.h>

#include <string>

class SRP {
public:
    SRP();
    ~SRP();

    void generate_verifier(const std::string& username, const std::string& password, std::string& out_salt_hex, std::string& out_verifier_hex);
    void generate_server_ephemeral();
    void process_client_public(const std::string& A_hex);
    bool verify_proof(const std::string& client_M1_hex);

    void load_verifier(const std::string& salt_hex, const std::string& verifier_hex);

    void generate_fake_challenge();
    void generate_verifier_from_proof(const std::string& A_hex, const std::string& client_M1_hex, std::string& out_salt_hex, std::string& out_verifier_hex);

    std::string get_salt() const { return salt; }
    std::string get_B() const { return bn_to_hex_str(B); }

private:
    BIGNUM *N;
    BIGNUM *g;
    BIGNUM *v;

    BIGNUM *b;  // server private
    BIGNUM *B;  // server public
    BIGNUM *A;  // client public
    BIGNUM *u;  // scrambling
    BIGNUM *S;  // session key

    std::string salt;

    BN_CTX *ctx;

    void hash_sha1(const std::string& input, unsigned char output[SHA_DIGEST_LENGTH]) const;
    BIGNUM* hash_bn(const std::string& input) const;
    std::string bn_to_hex_str(const BIGNUM* bn) const;
    static std::string bytes_to_hex(const unsigned char* bytes, size_t len);
};
