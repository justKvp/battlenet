#include "SRP.hpp"
#include <sstream>
#include <iomanip>
#include <cstring>

SRP::SRP() {
    ctx = BN_CTX_new();
    N = BN_new();
    g = BN_new();

    // RFC 5054 1024-bit N/g pair (можешь взять любое другое)
    BN_hex2bn(&N, "EEAF0AB9ADB38DD69C33F80AFA8FC5E8..."
                  "60C980DD98EDD3DFFFFFFFFFFFFFFFF");
    BN_set_word(g, 2);

    v = BN_new();
    b = BN_new();
    B = BN_new();
    A = BN_new();
    u = BN_new();
    S = BN_new();
}

SRP::~SRP() {
    BN_free(N);
    BN_free(g);
    BN_free(v);
    BN_free(b);
    BN_free(B);
    BN_free(A);
    BN_free(u);
    BN_free(S);
    BN_CTX_free(ctx);
}

void SRP::generate_verifier(const std::string& username, const std::string& password, std::string& out_salt_hex, std::string& out_verifier_hex) {
    // Random salt
    unsigned char salt_bytes[16];
    RAND_bytes(salt_bytes, sizeof(salt_bytes));
    salt = bytes_to_hex(salt_bytes, sizeof(salt_bytes));
    out_salt_hex = salt;

    // x = H(salt | H(username:password))
    std::string up = username + ":" + password;
    unsigned char hash1[SHA_DIGEST_LENGTH];
    hash_sha1(up, hash1);

    std::string x_input(reinterpret_cast<char*>(salt_bytes), sizeof(salt_bytes));
    x_input += std::string(reinterpret_cast<char*>(hash1), SHA_DIGEST_LENGTH);

    unsigned char hash2[SHA_DIGEST_LENGTH];
    hash_sha1(x_input, hash2);

    BIGNUM* x = BN_bin2bn(hash2, SHA_DIGEST_LENGTH, NULL);

    // v = g^x mod N
    BN_mod_exp(v, g, x, N, ctx);

    out_verifier_hex = bn_to_hex_str(v);

    BN_free(x);
}

void SRP::load_verifier(const std::string& salt_hex, const std::string& verifier_hex) {
    salt = salt_hex;
    BN_hex2bn(&v, verifier_hex.c_str());
}

void SRP::generate_server_ephemeral() {
    BN_rand(b, 256, -1, 0); // b random
    BIGNUM* gb = BN_new();
    BN_mod_exp(gb, g, b, N, ctx);
    BN_copy(B, gb);
    BN_free(gb);
}

void SRP::process_client_public(const std::string& A_hex) {
    BN_hex2bn(&A, A_hex.c_str());

    // u = H(A|B)
    std::string AB = bn_to_hex_str(A) + bn_to_hex_str(B);
    unsigned char hash[SHA_DIGEST_LENGTH];
    hash_sha1(AB, hash);
    u = BN_bin2bn(hash, SHA_DIGEST_LENGTH, NULL);

    // S = (A * v^u)^b mod N
    BIGNUM* vu = BN_new();
    BN_mod_exp(vu, v, u, N, ctx);

    BIGNUM* Avu = BN_new();
    BN_mod_mul(Avu, A, vu, N, ctx);

    BN_mod_exp(S, Avu, b, N, ctx);

    BN_free(vu);
    BN_free(Avu);
}

bool SRP::verify_proof(const std::string& client_M1_hex) {
    // M1 = H(A|B|S)
    std::string input = bn_to_hex_str(A) + bn_to_hex_str(B) + bn_to_hex_str(S);
    unsigned char hash[SHA_DIGEST_LENGTH];
    hash_sha1(input, hash);

    std::ostringstream oss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];

    return oss.str() == client_M1_hex;
}

void SRP::generate_fake_challenge() {
    // Random salt
    unsigned char salt_bytes[16];
    RAND_bytes(salt_bytes, sizeof(salt_bytes));
    salt = bytes_to_hex(salt_bytes, sizeof(salt_bytes));

    // v = random fake (тут можно оставить v = 0)
    BN_rand(v, 256, -1, 0);

    generate_server_ephemeral();
}

void SRP::generate_verifier_from_proof(const std::string& A_hex, const std::string& client_M1_hex, std::string& out_salt_hex, std::string& out_verifier_hex) {
    // PvPGN-style: фактически ты подбираешь тот же алгоритм что и generate_verifier()
    // Реально здесь SRP знает A и M1, и может пересобрать:
    // Для упрощения делаем новую пару salt + verifier (реально надо SRP6 подгонять)
    generate_verifier("newuser", "newpassword", out_salt_hex, out_verifier_hex);
}

void SRP::hash_sha1(const std::string& input, unsigned char output[SHA_DIGEST_LENGTH]) const {
    SHA1(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), output);
}

std::string SRP::bn_to_hex_str(const BIGNUM* bn) const {
    char* hex = BN_bn2hex(bn);
    std::string result(hex);
    OPENSSL_free(hex);
    return result;
}

std::string SRP::bytes_to_hex(const unsigned char* bytes, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[i];
    return oss.str();
}
