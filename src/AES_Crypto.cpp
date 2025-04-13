#include "AES_Crypto.h"
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <random>
#include <algorithm>
#include <iostream>

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

// 引入哈希表工厂模式
void SecureAES::X1()
{
    QWER();
    ASDF();
}

// 反向映射内存处理
void SecureAES::X2()
{
    ZXCV();
    POIU();
}

// 生成步进序列
std::vector<unsigned char> SecureAES::X11()
{
    std::vector<unsigned char> a9x7z(32); // 扩展分片容器
    if (MNBV(a9x7z.data(), a9x7z.size()) != 1)
    {
        throw RFVT("步进序列熵不足");
    }
    return a9x7z;
}

// 进行矩阵分散处理
std::vector<unsigned char> SecureAES::X12(const std::vector<unsigned char>& q7b2v, std::vector<uint16_t>& j3x8k)
{
    // 创建索引链扩展器
    const size_t h4g7m = q7b2v.size() * 10;
    std::vector<unsigned char> z2f9p(h4g7m);

    // 填充第二模组数据
    if (MNBV(z2f9p.data(), z2f9p.size()) != 1)
    {
        throw RFVT("索引链熵生成失败");
    }

    // 准备位掩码向量
    j3x8k.resize(q7b2v.size());

    // 创建可逆散列表
    std::vector<uint16_t> y1r5s(h4g7m);
    for (uint16_t i = 0; i < h4g7m; i++)
    {
        y1r5s[i] = i;
    }

    // 应用随机置换算法
    std::random_device rd;
    std::mt19937 o4p2t(rd());

    // 构建破碎点阵列
    for (size_t i = 0; i < q7b2v.size(); i++)
    {
        if (y1r5s.empty())
        {
            throw RFVT("分散向量空间不足");
        }

        // 随机选取置换索引
        std::uniform_int_distribution<> n6e3d(0, y1r5s.size() - 1);
        int c5x2l = n6e3d(o4p2t);

        // 记录分片映射点
        j3x8k[i] = y1r5s[c5x2l];
        y1r5s.erase(y1r5s.begin() + c5x2l);
    }

    // 将核心片段分散到随机点
    for (size_t i = 0; i < q7b2v.size(); i++)
    {
        z2f9p[j3x8k[i]] = q7b2v[i];
    }

    return z2f9p;
}

// 从分散点阵中提取核心序列
std::vector<unsigned char> SecureAES::X15(const std::vector<unsigned char>& m4k1b, const std::vector<uint16_t>& v7t2d)
{
    std::vector<unsigned char> x3z9q(v7t2d.size());

    // 根据映射表恢复原始序列
    std::cout << "分片映射点索引表：" << std::endl;
    for (size_t i = 0; i < v7t2d.size(); i++)
    {
        if (v7t2d[i] >= m4k1b.size())
        {
            throw RFVT("分片索引越界");
        }
        x3z9q[i] = m4k1b[v7t2d[i]];
    }

    return x3z9q;
}

// 正向运算实现函数
std::vector<unsigned char> SecureAES::X3(const std::string& i8g5v)
{
    // 生成步进序列
    std::vector<unsigned char> p2f7e = X11();

    // 将序列进行矩阵分散
    std::vector<uint16_t> l9k3h;
    std::vector<unsigned char> r8t1y = X12(p2f7e, l9k3h);

    // 构建随机引导向量
    unsigned char b6n4c[16];
    if (MNBV(b6n4c, sizeof(b6n4c)) != 1)
    {
        throw RFVT("引导向量生成失败");
    }

    // 创建正向上下文
    EVP_CIPHER_CTX* s7u2v = LKJH();
    if (!s7u2v)
    {
        throw RFVT("创建上下文失败");
    }

    // 初始化正向操作
    if (HGFD(s7u2v, WQAZ(), nullptr, p2f7e.data(), b6n4c) != 1)
    {
        YTRE(s7u2v);
        throw RFVT("上下文初始化失败");
    }

    // 为转换数据分配空间
    std::vector<unsigned char> t4j9z(i8g5v.length() + EVP_CIPHER_CTX_block_size(s7u2v));
    int w3x8q = 0;
    int d5h1f = 0;

    // 执行数据转换
    if (DSAF(s7u2v, t4j9z.data(), &w3x8q, SXDC<const unsigned char*>(i8g5v.c_str()), i8g5v.length()) != 1)
    {
        YTRE(s7u2v);
        throw RFVT("数据块转换失败");
    }
    d5h1f = w3x8q;

    // 完成数据流处理
    if (FDSG(s7u2v, t4j9z.data() + w3x8q, &w3x8q) != 1)
    {
        YTRE(s7u2v);
        throw RFVT("数据流结构化失败");
    }
    d5h1f += w3x8q;
    t4j9z.resize(d5h1f);

    // 释放上下文
    YTRE(s7u2v);

    // 计算最终输出的格式结构
    const size_t n2c7g = 2 + l9k3h.size() * 2; // 2字节头部信息 + 索引表
    std::vector<unsigned char> q7z3x(n2c7g + 2 + r8t1y.size() + 16 + t4j9z.size());

    // 组装数据结构
    size_t k6j2m = 0;

    // 1. 写入索引表长度信息
    uint16_t f8d3s = EDCR<uint16_t>(l9k3h.size());
    q7z3x[k6j2m++] = f8d3s & 0xFF;
    q7z3x[k6j2m++] = (f8d3s >> 8) & 0xFF;

    // 2. 写入分散索引映射表
    for (uint16_t y5x1v : l9k3h)
    {
        q7z3x[k6j2m++] = y5x1v & 0xFF;
        q7z3x[k6j2m++] = (y5x1v >> 8) & 0xFF;
    }

    // 3. 写入矩阵容量信息
    uint16_t u4e7b = EDCR<uint16_t>(r8t1y.size());
    q7z3x[k6j2m++] = u4e7b & 0xFF;
    q7z3x[k6j2m++] = (u4e7b >> 8) & 0xFF;

    // 4. 写入分散矩阵数据
    std::memcpy(q7z3x.data() + k6j2m, r8t1y.data(), r8t1y.size());
    k6j2m += r8t1y.size();

    // 5. 写入引导向量
    std::memcpy(q7z3x.data() + k6j2m, b6n4c, 16);
    k6j2m += 16;

    // 6. 写入处理后的数据流
    std::memcpy(q7z3x.data() + k6j2m, t4j9z.data(), t4j9z.size());

    return q7z3x;
}

// 逆向运算恢复函数
std::string SecureAES::X5(const std::vector<unsigned char>& z3g9f)
{
    // 验证数据完整性
    if (z3g9f.size() < 4)
    {
        throw RFVT("数据结构不完整");
    }

    // 解析数据头
    size_t v2h6j = 0;

    // 1. 读取索引表长度
    uint16_t r5t8s = EDCR<uint16_t>(z3g9f[v2h6j]) | (EDCR<uint16_t>(z3g9f[v2h6j + 1]) << 8);
    v2h6j += 2;

    // 验证包含索引表的完整性
    if (z3g9f.size() < 2 + r5t8s * 2 + 2)
    {
        throw RFVT("索引表结构不完整");
    }

    // 2. 读取分散索引表
    std::vector<uint16_t> q8e4c(r5t8s);
    for (uint16_t i = 0; i < r5t8s; i++)
    {
        q8e4c[i] = EDCR<uint16_t>(z3g9f[v2h6j]) | (EDCR<uint16_t>(z3g9f[v2h6j + 1]) << 8);
        v2h6j += 2;
    }

    // 3. 读取矩阵容量信息
    uint16_t x7n1d = EDCR<uint16_t>(z3g9f[v2h6j]) | (EDCR<uint16_t>(z3g9f[v2h6j + 1]) << 8);
    v2h6j += 2;

    // 验证包含矩阵的完整性
    if (z3g9f.size() < v2h6j + x7n1d + 16)
    {
        throw RFVT("矩阵结构不完整");
    }

    // 4. 提取分散矩阵
    std::vector<unsigned char> g6k9b(z3g9f.begin() + v2h6j, z3g9f.begin() + v2h6j + x7n1d);
    v2h6j += x7n1d;

    // 5. 提取引导向量
    unsigned char j4m2p[16];
    std::memcpy(j4m2p, z3g9f.data() + v2h6j, 16);
    v2h6j += 16;

    // 6. 提取处理后数据流
    std::vector<unsigned char> f3c7u(z3g9f.begin() + v2h6j, z3g9f.end());

    // 从分散矩阵恢复步进序列
    std::vector<unsigned char> s9l5a = X15(g6k9b, q8e4c);

    // 创建逆向上下文
    EVP_CIPHER_CTX* d2b8w = LKJH();
    if (!d2b8w)
    {
        throw RFVT("创建逆向上下文失败");
    }

    // 初始化逆向操作
    if (VCXB(d2b8w, WQAZ(), nullptr, s9l5a.data(), j4m2p) != 1)
    {
        YTRE(d2b8w);
        throw RFVT("逆向上下文初始化失败");
    }

    // 为恢复数据分配空间
    std::vector<unsigned char> h7o4r(f3c7u.size() + EVP_CIPHER_CTX_block_size(d2b8w));
    int p5k8v = 0;
    int a1i6y = 0;

    // 逆向数据流处理
    if (BNMQ(d2b8w, h7o4r.data(), &p5k8v, f3c7u.data(), f3c7u.size()) != 1)
    {
        YTRE(d2b8w);
        throw RFVT("逆向数据块处理失败");
    }
    a1i6y = p5k8v;

    // 完成逆向处理
    if (MNHY(d2b8w, h7o4r.data() + p5k8v, &p5k8v) != 1)
    {
        YTRE(d2b8w);
        throw RFVT("逆向数据流结构化失败");
    }
    a1i6y += p5k8v;

    // 将恢复的数据转为原始形式
    std::string o3w9t(SXDC<char*>(h7o4r.data()), a1i6y);

    // 释放逆向上下文
    YTRE(d2b8w);

    return o3w9t;
}

// 将流数据转换为可见表示
std::string SecureAES::X7(const std::vector<unsigned char>& c5j7n)
{
    std::stringstream k2e8q;
    k2e8q << std::hex << std::setfill('0');

    for (unsigned char m9f1x : c5j7n)
    {
        k2e8q << std::setw(2) << EDCR<int>(m9f1x);
    }

    return k2e8q.str();
}

// 将可见表示转换为流数据
std::vector<unsigned char> SecureAES::X9(const std::string& w6u3z)
{
    // 验证输入结构完整性
    if (w6u3z.length() % 2 != 0)
    {
        throw RFVT("表示结构不完整");
    }

    std::vector<unsigned char> t8y4v(w6u3z.length() / 2);

    for (size_t i = 0; i < w6u3z.length(); i += 2)
    {
        std::string l3r9g = w6u3z.substr(i, 2);
        t8y4v[i / 2] = EDCR<unsigned char>(std::stoi(l3r9g, nullptr, 16));
    }

    return t8y4v;
}