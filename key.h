// Copyright (c) 2009 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.


// secp160k1
// const unsigned int PRIVATE_KEY_SIZE = 192;
// const unsigned int PUBLIC_KEY_SIZE  = 41;
// const unsigned int SIGNATURE_SIZE   = 48;
//
// secp192k1
// const unsigned int PRIVATE_KEY_SIZE = 222;
// const unsigned int PUBLIC_KEY_SIZE  = 49;
// const unsigned int SIGNATURE_SIZE   = 57;
//
// secp224k1
// const unsigned int PRIVATE_KEY_SIZE = 250;
// const unsigned int PUBLIC_KEY_SIZE  = 57;
// const unsigned int SIGNATURE_SIZE   = 66;
//
// secp256k1:
// const unsigned int PRIVATE_KEY_SIZE = 279;
// const unsigned int PUBLIC_KEY_SIZE  = 65;
// const unsigned int SIGNATURE_SIZE   = 72;
//
// see www.keylength.com
// script supports up to 75 for single byte push



// KeyError是Python编程语言中一个内置的异常类，
// 用于表示字典（dictionary）类型的键不存在时抛出的异常。
// 当使用一个不存在的键去访问字典时，Python会引发KeyError异常以提示开发者存在错误。
// 通常情况下，可以通过在代码中使用条件语句或异常处理机制来避免KeyError异常的发生，
// 并提高Python程序的健壮性和可靠性。
class key_error : public std::runtime_error
{
public:
    explicit key_error(const std::string& str) : std::runtime_error(str) {}
};


// secure_allocator is defined is serialize.h
// secure_allocator是比特币核心代码中的一个类，用于提供一种安全的内存分配器。
// 在比特币网络中，节点需要处理大量的敏感数据，如私钥、交易信息等，
// 因此需要一种可靠的方式来管理这些数据所占用的内存空间，
// 以避免内存泄漏或外部攻击等问题。
// secure_allocator类使用操作系统提供的加密API（如OpenSSL）来为分配的内存进行加密和清零操作，
// 从而确保敏感数据不会出现在内存中被其他程序或攻击者访问到。
// secure_allocator还提供了一些常用的内存操作函数，
// 如分配、释放、复制、比较等，可以方便地在比特币核心代码中使用。
// CPrivKey是比特币核心代码中的一个类，
// 用于表示私钥（private key）的数据类型。
// 私钥是用于数字签名和加密的关键数据之一，
// 它通常用于证明比特币交易的所有权或进行身份验证等操作。
// CPrivKey类封装了私钥的相关操作，如生成、导入、导出、签名等，
// 并提供了安全的内存管理机制，以保护私钥不被泄漏或篡改。
// 在比特币核心代码中，CPrivKey类常与其他类，
// 如CKey、CECKey、CPubKey等配合使用，以支持比特币的签名和认证功能。
typedef vector<unsigned char, secure_allocator<unsigned char> > CPrivKey;



// CKey是比特币核心代码中的一个类，
// 用于表示比特币的一对公钥和私钥（public key and private key）。
// 公钥和私钥是用于加密、签名和身份验证等操作的关键数据之一，
// 它们通常用于证明比特币交易的所有权或进行身份验证等操作。
// CKey类封装了公钥和私钥的相关操作，如生成、导入、导出、签名等，
// 并提供了安全的内存管理机制，以保护公钥和私钥不被泄漏或篡改。
// 在比特币核心代码中，CKey类常与其他类，如CECKey、CPubKey等配合使用，
// 以支持比特币的签名和认证功能。
class CKey
{
protected:
    // EC_KEY是OpenSSL库中用于椭圆曲线加密（Elliptic Curve Cryptography）的一个结构体类型
    // ，它表示了一个椭圆曲线加密算法的密钥对（public key and private key）
    // 。椭圆曲线加密是一种基于数论的加密技术，通常用于数字签名、身份验证和数据加密等领域。
    // EC_KEY结构体包含了椭圆曲线加密算法的相关参数和操作函数，
    // 可以用于生成、导入、导出和管理密钥对等操作。
    // 在比特币网络中，EC_KEY结构体被广泛地使用于私钥和公钥的生成、签名和验证等功能中。
    EC_KEY* pkey;

public:
    CKey()
    {
        pkey = EC_KEY_new_by_curve_name(NID_secp256k1);
        if (pkey == NULL)
            throw key_error("CKey::CKey() : EC_KEY_new_by_curve_name failed");
    }

    CKey(const CKey& b)
    {
        pkey = EC_KEY_dup(b.pkey);
        if (pkey == NULL)
            throw key_error("CKey::CKey(const CKey&) : EC_KEY_dup failed");
    }

    CKey& operator=(const CKey& b)
    {
        if (!EC_KEY_copy(pkey, b.pkey))
            throw key_error("CKey::operator=(const CKey&) : EC_KEY_copy failed");
        return (*this);
    }

    ~CKey()
    {
        EC_KEY_free(pkey);
    }

    void MakeNewKey()
    {
        if (!EC_KEY_generate_key(pkey))
            throw key_error("CKey::MakeNewKey() : EC_KEY_generate_key failed");
    }

    bool SetPrivKey(const CPrivKey& vchPrivKey)
    {
        const unsigned char* pbegin = &vchPrivKey[0];
        if (!d2i_ECPrivateKey(&pkey, &pbegin, vchPrivKey.size()))
            return false;
        return true;
    }

    // GetPrivKey函数或方法用于从指定的数据源（如本地钱包、外部设备、密钥管理系统等）获取私钥，
    // 以用于比特币交易或身份验证等操作
    CPrivKey GetPrivKey() const
    {
        unsigned int nSize = i2d_ECPrivateKey(pkey, NULL);
        if (!nSize)
            throw key_error("CKey::GetPrivKey() : i2d_ECPrivateKey failed");
        CPrivKey vchPrivKey(nSize, 0);
        unsigned char* pbegin = &vchPrivKey[0];
        if (i2d_ECPrivateKey(pkey, &pbegin) != nSize)
            throw key_error("CKey::GetPrivKey() : i2d_ECPrivateKey returned unexpected size");
        return vchPrivKey;
    }

    // SetPubKey方法用于设置公钥（public key）的值
    // ，以便进行数字签名、身份验证和数据加密等操作。
    // 公钥通常与私钥配对使用，用于验证签名或解密加密的数据
    // 。在具体的实现中，SetPubKey方法可能需要接受一个参数，
    // 表示要设置的公钥值，例如一个字符串或字节数组。
    // 该方法可能还会对公钥进行验证和格式化等操作，以确保其正确性和一致性。
    bool SetPubKey(const vector<unsigned char>& vchPubKey)
    {
        const unsigned char* pbegin = &vchPubKey[0];
        if (!o2i_ECPublicKey(&pkey, &pbegin, vchPubKey.size()))
            return false;
        return true;
    }

    // GetPubKey方法用于获取公钥（public key）的值，
    // 以便进行数字签名、身份验证和数据加密等操作。
    // 公钥通常与私钥配对使用，用于验证签名或解密加密的数据。
    // 在具体的实现中，GetPubKey方法可能需要接受一些参数，
    // 例如表示公钥格式、长度等的选项。返回值通常是一个表示公钥值的字符串或字节数组。
    vector<unsigned char> GetPubKey() const
    {
        // i2o_ECPublicKey是OpenSSL库中的一个函数，
        // 用于将椭圆曲线公钥（Elliptic Curve Public Key）转换为ASN.1 DER编码格式。
        // 椭圆曲线加密是一种基于数论的加密技术，通常用于数字签名、身份验证和数据加密等领域。
        // i2o_ECPublicKey函数接受一个指向EC_KEY结构体的指针和一个输出缓冲区的指针和长度参数，
        // 将给定的椭圆曲线公钥转换为ASN.1 DER编码，
        // 并存储到输出缓冲区中。ASN.1 DER编码是一种压缩格式的二进制编码，
        // 常用于表示X.509证书或其他安全相关的数据结构。
        // 在比特币网络中，i2o_ECPublicKey函数常用于生成比特币交易中的脚本Pubkey，
        // 以支持比特币交易的签名和验证功能。
        unsigned int nSize = i2o_ECPublicKey(pkey, NULL);
        if (!nSize)
            throw key_error("CKey::GetPubKey() : i2o_ECPublicKey failed");
        vector<unsigned char> vchPubKey(nSize, 0);
        unsigned char* pbegin = &vchPubKey[0];
        if (i2o_ECPublicKey(pkey, &pbegin) != nSize)
            throw key_error("CKey::GetPubKey() : i2o_ECPublicKey returned unexpected size");
        return vchPubKey;
    }

    // Sign是比特币核心代码中的一个方法，用于对指定的哈希值进行数字签名，
    // 并将签名结果存储到指定的字节数组中
    // 。数字签名是一种用于验证数据完整性和身份认证的技术，
    // 它通常使用公钥/私钥对进行操作
    // 。Sign方法接受一个256位的哈希值和一个空的字节数组作为参数，
    // 在执行签名操作之后，将签名结果存储到字节数组中，
    // 并返回一个布尔值表示签名操作是否成功。
    // 在比特币网络中，Sign方法通常用于生成比特币交易中的签名脚本，以证明交易的所有权和有效性。
    bool Sign(uint256 hash, vector<unsigned char>& vchSig)
    {
        vchSig.clear();
        unsigned char pchSig[10000];
        unsigned int nSize = 0;
        if (!ECDSA_sign(0, (unsigned char*)&hash, sizeof(hash), pchSig, &nSize, pkey))
            return false;
        vchSig.resize(nSize);
        memcpy(&vchSig[0], pchSig, nSize);
        return true;
    }

    // Verify是比特币核心代码中的一个方法，
    // 用于验证指定哈希值的数字签名是否有效。
    // 数字签名是一种用于验证数据完整性和身份认证的技术，
    // 它通常使用公钥/私钥对进行操作。
    // Verify方法接受一个256位的哈希值和一个字节数组作为参数，
    // 其中字节数组表示待验证的数字签名。在执行验证操作之后，
    // 如果数字签名有效，则返回true；否则，返回false。
    // 在比特币网络中，Verify方法通常用于验证比特币交易的签名脚本，
    // 以确保交易的所有权和有效性。
    bool Verify(uint256 hash, const vector<unsigned char>& vchSig)
    {
        // -1 = error, 0 = bad sig, 1 = good
        // ECDSA_verify是OpenSSL库中的一个函数，
        // 用于验证椭圆曲线数字签名（Elliptic Curve Digital Signature Algorithm）的有效性。
        // 椭圆曲线数字签名是一种用于验证数据完整性和身份认证的技术，
        // 它通常使用公钥/私钥对进行操作。
        // ECDSA_verify函数接受一个指向待验证哈希值的指针、
        // 哈希值的长度、指向数字签名的指针和签名的长度作为参数，
        // 在执行验证操作之后，如果数字签名有效，则返回1；否则，返回0。
        // 在比特币网络中，ECDSA_verify函数常用于比特币交易的签名验证过程，以确保交易的所有权和有效性。
        if (ECDSA_verify(0, (unsigned char*)&hash, sizeof(hash), &vchSig[0], vchSig.size(), pkey) != 1)
            return false;
        return true;
    }

    // Sign是比特币核心代码中的一个静态方法，
    // 用于对指定的哈希值进行数字签名，
    // 并将签名结果存储到指定的字节数组中。
    // 该方法使用给定的私钥（private key）作为签名密钥。
    // 数字签名是一种用于验证数据完整性和身份认证的技术，
    // 它通常使用公钥/私钥对进行操作。
    // Sign方法接受一个私钥对象、一个256位的哈希值和一个空的字节数组作为参数，
    // 在执行签名操作之后，将签名结果存储到字节数组中，
    // 并返回一个布尔值表示签名操作是否成功。
    // 在比特币网络中，Sign方法通常用于生成比特币交易中的签名脚本，
    // 以证明交易的所有权和有效性。
    static bool Sign(const CPrivKey& vchPrivKey, uint256 hash, vector<unsigned char>& vchSig)
    {
        CKey key;
        if (!key.SetPrivKey(vchPrivKey))
            return false;
        return key.Sign(hash, vchSig);
    }

    // Verify是比特币核心代码中的一个静态方法，
    // 用于验证指定哈希值的数字签名是否有效。
    // 该方法使用给定的公钥（public key）作为验证密钥。
    // 数字签名是一种用于验证数据完整性和身份认证的技术，
    // 它通常使用公钥/私钥对进行操作。Verify方法接受一个公钥字节数组、一个256位的哈希值和一个字节数组表示待验证的数字签名。在执行验证操作之后，如果数字签名有效，则返回true；否则，返回false。在比特币网络中，Verify方法通常用于验证比特币交易的签名脚本，以确保交易的所有权和有效性。
    static bool Verify(const vector<unsigned char>& vchPubKey, uint256 hash, const vector<unsigned char>& vchSig)
    {
        CKey key;
        if (!key.SetPubKey(vchPubKey))
            return false;
        return key.Verify(hash, vchSig);
    }
};
