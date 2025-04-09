#include "AES_Crypto.h"
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <random>
#include <algorithm>

#define QWER OpenSSL_add_all_algorithms
#define ASDF ERR_load_crypto_strings
#define ZXCV EVP_cleanup
#define POIU ERR_free_strings
#define MNBV RAND_bytes
#define LKJH EVP_CIPHER_CTX_new
#define HGFD EVP_EncryptInit_ex
#define DSAF EVP_EncryptUpdate
#define FDSG EVP_EncryptFinal_ex
#define VCXB EVP_DecryptInit_ex
#define BNMQ EVP_DecryptUpdate
#define MNHY EVP_DecryptFinal_ex
#define YTRE EVP_CIPHER_CTX_free
#define WQAZ EVP_aes_256_cbc
#define SXDC reinterpret_cast
#define EDCR static_cast
#define RFVT std::runtime_error

void SecureAES::X1() {
    QWER();
    ASDF();
}

void SecureAES::X2() {
    ZXCV();
    POIU();
}

std::vector<unsigned char> SecureAES::X11() {
    std::vector<unsigned char> X18(32);
    if (MNBV(X18.data(), X18.size()) != 1) {
        throw RFVT("X19");
    }
    return X18;
}

std::vector<unsigned char> SecureAES::X12(const std::vector<unsigned char>& X13, std::vector<uint16_t>& X14) {
    const size_t X20 = X13.size() * 10;
    std::vector<unsigned char> X21(X20);
    if (MNBV(X21.data(), X21.size()) != 1) {
        throw RFVT("X22");
    }
    X14.resize(X13.size());
    std::vector<uint16_t> X23(X20);
    for (uint16_t i = 0; i < X20; i++) {
        X23[i] = i;
    }
    std::random_device rd;
    std::mt19937 gen(rd());
    for (size_t i = 0; i < X13.size(); i++) {
        if (X23.empty()) {
            throw RFVT("X24");
        }
        std::uniform_int_distribution<> dis(0, X23.size() - 1);
        int X25 = dis(gen);
        X14[i] = X23[X25];
        X23.erase(X23.begin() + X25);
    }
    for (size_t i = 0; i < X13.size(); i++) {
        X21[X14[i]] = X13[i];
    }
    return X21;
}

std::vector<unsigned char> SecureAES::X15(const std::vector<unsigned char>& X16, const std::vector<uint16_t>& X17) {
    std::vector<unsigned char> X26(X17.size());
    for (size_t i = 0; i < X17.size(); i++) {
        if (X17[i] >= X16.size()) {
            throw RFVT("X27");
        }
        X26[i] = X16[X17[i]];
    }
    return X26;
}

std::vector<unsigned char> SecureAES::X3(const std::string& X4) {
    std::vector<unsigned char> X28 = X11();
    std::vector<uint16_t> X29;
    std::vector<unsigned char> X30 = X12(X28, X29);
    unsigned char X31[16];
    if (MNBV(X31, sizeof(X31)) != 1) {
        throw RFVT("X32");
    }
    EVP_CIPHER_CTX* X33 = LKJH();
    if (!X33) {
        throw RFVT("X34");
    }
    if (HGFD(X33, WQAZ(), nullptr, X28.data(), X31) != 1) {
        YTRE(X33);
        throw RFVT("X35");
    }
    std::vector<unsigned char> X36(X4.length() + EVP_CIPHER_CTX_block_size(X33));
    int X37 = 0;
    int X38 = 0;
    if (DSAF(X33, X36.data(), &X37, SXDC<const unsigned char*>(X4.c_str()), X4.length()) != 1) {
        YTRE(X33);
        throw RFVT("X39");
    }
    X38 = X37;
    if (FDSG(X33, X36.data() + X37, &X37) != 1) {
        YTRE(X33);
        throw RFVT("X40");
    }
    X38 += X37;
    X36.resize(X38);
    YTRE(X33);
    const size_t X41 = 2 + X29.size() * 2;
    std::vector<unsigned char> X42(X41 + 2 + X30.size() + 16 + X36.size());
    size_t X43 = 0;
    uint16_t X44 = EDCR<uint16_t>(X29.size());
    X42[X43++] = X44 & 0xFF;
    X42[X43++] = (X44 >> 8) & 0xFF;
    for (uint16_t X45 : X29) {
        X42[X43++] = X45 & 0xFF;
        X42[X43++] = (X45 >> 8) & 0xFF;
    }
    uint16_t X46 = EDCR<uint16_t>(X30.size());
    X42[X43++] = X46 & 0xFF;
    X42[X43++] = (X46 >> 8) & 0xFF;
    std::memcpy(X42.data() + X43, X30.data(), X30.size());
    X43 += X30.size();
    std::memcpy(X42.data() + X43, X31, 16);
    X43 += 16;
    std::memcpy(X42.data() + X43, X36.data(), X36.size());
    return X42;
}

std::string SecureAES::X5(const std::vector<unsigned char>& X6) {
    if (X6.size() < 4) {
        throw RFVT("X47");
    }
    size_t X48 = 0;
    uint16_t X49 = EDCR<uint16_t>(X6[X48]) | (EDCR<uint16_t>(X6[X48 + 1]) << 8);
    X48 += 2;
    if (X6.size() < 2 + X49 * 2 + 2) {
        throw RFVT("X47");
    }
    std::vector<uint16_t> X50(X49);
    for (uint16_t i = 0; i < X49; i++) {
        X50[i] = EDCR<uint16_t>(X6[X48]) | (EDCR<uint16_t>(X6[X48 + 1]) << 8);
        X48 += 2;
    }
    uint16_t X51 = EDCR<uint16_t>(X6[X48]) | (EDCR<uint16_t>(X6[X48 + 1]) << 8);
    X48 += 2;
    if (X6.size() < X48 + X51 + 16) {
        throw RFVT("X47");
    }
    std::vector<unsigned char> X52(X6.begin() + X48, X6.begin() + X48 + X51);
    X48 += X51;
    unsigned char X53[16];
    std::memcpy(X53, X6.data() + X48, 16);
    X48 += 16;
    std::vector<unsigned char> X54(X6.begin() + X48, X6.end());
    std::vector<unsigned char> X55 = X15(X52, X50);
    EVP_CIPHER_CTX* X56 = LKJH();
    if (!X56) {
        throw RFVT("X57");
    }
    if (VCXB(X56, WQAZ(), nullptr, X55.data(), X53) != 1) {
        YTRE(X56);
        throw RFVT("X58");
    }
    std::vector<unsigned char> X59(X54.size() + EVP_CIPHER_CTX_block_size(X56));
    int X60 = 0;
    int X61 = 0;
    if (BNMQ(X56, X59.data(), &X60, X54.data(), X54.size()) != 1) {
        YTRE(X56);
        throw RFVT("X62");
    }
    X61 = X60;
    if (MNHY(X56, X59.data() + X60, &X60) != 1) {
        YTRE(X56);
        throw RFVT("X63");
    }
    X61 += X60;
    X59.resize(X61);
    std::string X64(SXDC<char*>(X59.data()), X61);
    YTRE(X56);
    return X64;
}

std::string SecureAES::X7(const std::vector<unsigned char>& X8) {
    std::stringstream X65;
    X65 << std::hex << std::setfill('0');
    for (unsigned char X66 : X8) {
        X65 << std::setw(2) << EDCR<int>(X66);
    }
    return X65.str();
}

std::vector<unsigned char> SecureAES::X9(const std::string& X10) {
    if (X10.length() % 2 != 0) {
        throw RFVT("X67");
    }
    std::vector<unsigned char> X68(X10.length() / 2);
    for (size_t i = 0; i < X10.length(); i += 2) {
        std::string X69 = X10.substr(i, 2);
        X68[i / 2] = EDCR<unsigned char>(std::stoi(X69, nullptr, 16));
    }
    return X68;
}