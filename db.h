// Copyright (c) 2009 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include <db_cxx.h>
class CTransaction;
class CTxIndex;
class CDiskBlockIndex;
class CDiskTxPos;
class COutPoint;
class CUser;
class CReview;
class CAddress;
class CWalletTx;

// 存储已知地址和对应的标签信息
extern map<string, string> mapAddressBook;
extern bool fClient;


extern DbEnv dbenv;
extern void DBFlush(bool fShutdown);



// 用于封装和管理 Berkeley DB 数据库操作。
// 该类提供了一组方便的函数接口，可以进行数据库的打开、关闭、读取和写入等操作，
// 并支持事务和锁机制以确保数据的一致性和可靠性。
// 在比特币节点中，CDB 类通常用于管理区块链、钱包、地址本等数据存储，
// 以及网络消息传输等方面。
class CDB
{
protected:
    Db* pdb;
    // 用于标识要读取或写入的文件名或路径
    string strFile;
    // "DbTxn" 是 Berkeley DB 数据库中的一个结构体，
    // 用于封装数据库事务处理相关的信息。
    // 该结构体包含了事务 ID、锁定方式、读写模式、父事务等相关信息，
    // 可以用于控制并发访问和修改数据库时的一致性和可靠性。
    // 在比特币节点中，CDB 类使用 DbTxn 结构体来实现数据库事务处理，
    // 以确保在多线程环境下对数据库的读写操作具有原子性和隔离性。
    vector<DbTxn*> vTxn;

    explicit CDB(const char* pszFile, const char* pszMode="r+", bool fTxn=false);
    ~CDB() { Close(); }
public:
    void Close();
private:
    CDB(const CDB&);
    void operator=(const CDB&);

protected:
    template<typename K, typename T>
    bool Read(const K& key, T& value)
    {
        if (!pdb)
            return false;

        // Key
        CDataStream ssKey(SER_DISK);
        ssKey.reserve(1000);
        ssKey << key;
        // Dbt" 是 Berkeley DB 数据库中的一个结构体，
        // 用于封装数据库读写操作相关的数据和参数。
        // 该结构体包含了数据缓冲区、数据长度、偏移量等相关信息
        // ，可以用于指定要读取或写入的数据内容和数据格式等。
        // 在比特币节点中，CDB 类使用 Dbt 结构体来实现对数据库的读写操作，
        // 以便存储和查询区块链、钱包、地址本等重要数据。
        Dbt datKey(&ssKey[0], ssKey.size());

        // Read
        Dbt datValue;
        datValue.set_flags(DB_DBT_MALLOC);
        int ret = pdb->get(GetTxn(), &datKey, &datValue, 0);
        memset(datKey.get_data(), 0, datKey.get_size());
        if (datValue.get_data() == NULL)
            return false;

        // Unserialize value
        CDataStream ssValue((char*)datValue.get_data(), (char*)datValue.get_data() + datValue.get_size(), SER_DISK);
        ssValue >> value;

        // Clear and free memory
        memset(datValue.get_data(), 0, datValue.get_size());
        free(datValue.get_data());
        return (ret == 0);
    }

    template<typename K, typename T>
    bool Write(const K& key, const T& value, bool fOverwrite=true)
    {
        if (!pdb)
            return false;

        // Key
        CDataStream ssKey(SER_DISK);
        ssKey.reserve(1000);
        ssKey << key;
        Dbt datKey(&ssKey[0], ssKey.size());

        // Value
        CDataStream ssValue(SER_DISK);
        ssValue.reserve(10000);
        ssValue << value;
        Dbt datValue(&ssValue[0], ssValue.size());

        // Write
        int ret = pdb->put(GetTxn(), &datKey, &datValue, (fOverwrite ? 0 : DB_NOOVERWRITE));

        // Clear memory in case it was a private key
        memset(datKey.get_data(), 0, datKey.get_size());
        memset(datValue.get_data(), 0, datValue.get_size());
        return (ret == 0);
    }

    template<typename K>
    bool Erase(const K& key)
    {
        if (!pdb)
            return false;

        // Key
        CDataStream ssKey(SER_DISK);
        ssKey.reserve(1000);
        ssKey << key;
        Dbt datKey(&ssKey[0], ssKey.size());

        // Erase
        int ret = pdb->del(GetTxn(), &datKey, 0);

        // Clear memory
        memset(datKey.get_data(), 0, datKey.get_size());
        return (ret == 0 || ret == DB_NOTFOUND);
    }

    template<typename K>
    bool Exists(const K& key)
    {
        if (!pdb)
            return false;

        // Key
        CDataStream ssKey(SER_DISK);
        ssKey.reserve(1000);
        ssKey << key;
        Dbt datKey(&ssKey[0], ssKey.size());

        // Exists
        int ret = pdb->exists(GetTxn(), &datKey, 0);

        // Clear memory
        memset(datKey.get_data(), 0, datKey.get_size());
        return (ret == 0);
    }

    Dbc* GetCursor()
    {
        if (!pdb)
            return NULL;
        Dbc* pcursor = NULL;
        int ret = pdb->cursor(NULL, &pcursor, 0);
        if (ret != 0)
            return NULL;
        return pcursor;
    }

    int ReadAtCursor(Dbc* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags=DB_NEXT)
    {
        // Read at cursor
        Dbt datKey;
        if (fFlags == DB_SET || fFlags == DB_SET_RANGE || fFlags == DB_GET_BOTH || fFlags == DB_GET_BOTH_RANGE)
        {
            datKey.set_data(&ssKey[0]);
            datKey.set_size(ssKey.size());
        }
        Dbt datValue;
        if (fFlags == DB_GET_BOTH || fFlags == DB_GET_BOTH_RANGE)
        {
            datValue.set_data(&ssValue[0]);
            datValue.set_size(ssValue.size());
        }
        datKey.set_flags(DB_DBT_MALLOC);
        datValue.set_flags(DB_DBT_MALLOC);
        int ret = pcursor->get(&datKey, &datValue, fFlags);
        if (ret != 0)
            return ret;
        else if (datKey.get_data() == NULL || datValue.get_data() == NULL)
            return 99999;

        // Convert to streams
        ssKey.SetType(SER_DISK);
        ssKey.clear();
        ssKey.write((char*)datKey.get_data(), datKey.get_size());
        ssValue.SetType(SER_DISK);
        ssValue.clear();
        ssValue.write((char*)datValue.get_data(), datValue.get_size());

        // Clear and free memory
        memset(datKey.get_data(), 0, datKey.get_size());
        memset(datValue.get_data(), 0, datValue.get_size());
        free(datKey.get_data());
        free(datValue.get_data());
        return 0;
    }

    
    DbTxn* GetTxn()
    {
        if (!vTxn.empty())
            return vTxn.back();
        else
            return NULL;
    }

public:
    // "TxnBegin" 是 Berkeley DB 数据库中的一个函数，
    // 用于开始一个数据库事务。该函数会创建一个新的 DbTxn 对象，
    // 并返回一个表示该事务的事务句柄（TxnHandle）。
    // 在比特币节点中，CDB 类使用 TxnBegin 函数来启动对数据库的事务处理，
    // 在事务处理期间，所有对数据库的读写操作都将被记录到一个单独的事务日志文件中，
    // 并在事务提交时批量写入数据库，以确保数据的一致性和可靠性。
    bool TxnBegin()
    {
        if (!pdb)
            return false;
        DbTxn* ptxn = NULL;
        int ret = dbenv.txn_begin(GetTxn(), &ptxn, 0);
        if (!ptxn || ret != 0)
            return false;
        vTxn.push_back(ptxn);
        return true;
    }

    // "TxnCommit" 是 Berkeley DB 数据库中的一个函数，
    // 用于提交一个数据库事务。该函数会将当前事务所做出的所有更改写入数据库，
    // 并释放相关资源。在比特币节点中，CDB 类使用 TxnCommit 函数来提交对数据库的事务处理，
    // 并将数据写入磁盘，以便持久化存储区块链、钱包、地址本等重要数据。
    bool TxnCommit()
    {
        if (!pdb)
            return false;
        if (vTxn.empty())
            return false;
        int ret = vTxn.back()->commit(0);
        vTxn.pop_back();
        return (ret == 0);
    }

    // "TxnAbort" 是 Berkeley DB 数据库中的一个函数，
    // 用于终止一个数据库事务。该函数会回滚当前事务所做出的所有更改，
    // 并释放相关资源。在比特币节点中，CDB 类使用 TxnAbort 函数来撤销对数据库的事务处理，
    // 并回滚未提交的更改，以确保数据的一致性和完整性。
    bool TxnAbort()
    {
        if (!pdb)
            return false;
        if (vTxn.empty())
            return false;
        int ret = vTxn.back()->abort();
        vTxn.pop_back();
        return (ret == 0);
    }

    // 用于读取数据库版本信息
    // 。该函数在比特币节点启动时被调用，
    // 并从磁盘上的数据库文件中读取数据库版本号，
    // 以便检查数据库是否需要升级或降级。在比特币节点运行期间，
    // 如果有任何导致数据库版本不兼容的变更发生（例如升级到新版本的比特币软件），
    // 则会触发数据库升级操作以确保数据的一致性和可靠性。
    bool ReadVersion(int& nVersion)
    {
        nVersion = 0;
        return Read(string("version"), nVersion);
    }

    // 用于写入数据库版本信息。
    // 该函数在比特币节点启动时被调用
    // ，并将当前的数据库版本号写入磁盘上的数据库文件中，以便下一次启动时读取
    bool WriteVersion(int nVersion)
    {
        return Write(string("version"), nVersion);
    }
};







// "CTxDB" 是比特币源代码中的一个类，
// 用于管理交易相关的数据存储和操作。
// 该类封装了 Berkeley DB 数据库，提供了一组方便的函数接口，
// 可以将交易相关的信息写入磁盘，以及从磁盘读取这些信息并进行查询、更新等操作。在
// 比特币节点中，CTxDB 类扮演着重要的角色，用于保存和管理比特币网络上的所有交易记录
// ，包括未确认交易和已确认交易等。
class CTxDB : public CDB
{
public:
    CTxDB(const char* pszMode="r+", bool fTxn=false) : CDB(!fClient ? "blkindex.dat" : NULL, pszMode, fTxn) { }
private:
    CTxDB(const CTxDB&);
    void operator=(const CTxDB&);
public:
    // 用于从交易索引数据库中读取指定交易的索引信息。
    // 交易索引数据库包含了所有交易的哈希值和位置信息，可以用于快速查询和检索交易记录
    // 。在比特币节点中，ReadTxIndex 函数通常被用于获取特定交易的详细信息，
    // 例如交易输入和输出、区块高度、确认数等等。
    bool ReadTxIndex(uint256 hash, CTxIndex& txindex);
    // "UpdateTxIndex"可能指的是比特币区块链中的"TxIndex"选项，
    // 它允许在比特币核心客户端中启用事务索引。当启用此选项时
    // ，比特币客户端会将每个交易的哈希值与其位置（块高度和交易索引）关联起来，
    // 并在必要时使用该信息来快速查找特定交易。
    // 因此，"UpdateTxIndex"的作用是更新比特币客户端中的事务索引，
    // 以确保它包含每个新块中包含的交易的相关信息。这可以提高比特币客户端的性能
    // ，并使用户能够更快地查找和验证交易。
    bool UpdateTxIndex(uint256 hash, const CTxIndex& txindex);
    // "AddTxIndex" 可能是指比特币核心客户端中的一个选项，
    // 用于启用事务索引。当启用此选项时，
    // 比特币客户端将记录每个交易的哈希值和该交易在区块链中的位置（即块高度和交易索引）。
    // 这使得用户可以更快地查找和验证交易，并且可以通过交易哈希值跟踪特定交易的状态。
    // 因此，"AddTxIndex" 的作用是将事务索引添加到比特币客户端中，
    // 以便记录每个交易的相关信息，并使其更容易访问和使用。
    bool AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight);
    // "EraseTxIndex" 可能指的是比特币核心客户端中禁用事务索引的选项。
    // 当禁用此选项时，比特币客户端将不再记录每个交易的哈希值和该交易在区块链中的位置
    // （即块高度和交易索引）。
    // 因此，"EraseTxIndex" 的作用是从比特币客户端中删除事务索引，
    // 并停止记录每个交易的相关信息。这可能会导致用户无法快速查找或验证交易，
    // 因为必须手动搜索区块链来查找特定交易。但是，这也可以减少比特币客户端的存储需求，
    // 并降低其对系统资源的占用率。
    bool EraseTxIndex(const CTransaction& tx);
    bool ContainsTx(uint256 hash);
    bool ReadOwnerTxes(uint160 hash160, int nHeight, vector<CTransaction>& vtx);
    bool ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(uint256 hash, CTransaction& tx);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex);
    bool ReadDiskTx(COutPoint outpoint, CTransaction& tx);
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
    bool EraseBlockIndex(uint256 hash);
    bool ReadHashBestChain(uint256& hashBestChain);
    bool WriteHashBestChain(uint256 hashBestChain);
    bool LoadBlockIndex();
};




// 用于存储和管理区块链数据
// "CReviewDB" 可以在比特币客户端启动时自动创建，
// 并随着新块的添加而不断更新。它包含了比特币区块链中的所有交易和块的相关元数据信息，
// 例如交易哈希值、输入和输出脚本、块高度、时间戳等。
// 使用"CReviewDB"可以提高比特币客户端的性能，
// 并使其更容易访问和管理区块链数据。通过快速查找和访问需要的数据，
// 可以加快交易验证和处理速度，提高比特币网络的整体效率。
class CReviewDB : public CDB
{
public:
    CReviewDB(const char* pszMode="r+", bool fTxn=false) : CDB("reviews.dat", pszMode, fTxn) { }
private:
    CReviewDB(const CReviewDB&);
    void operator=(const CReviewDB&);
public:
    bool ReadUser(uint256 hash, CUser& user)
    {
        return Read(make_pair(string("user"), hash), user);
    }

    bool WriteUser(uint256 hash, const CUser& user)
    {
        return Write(make_pair(string("user"), hash), user);
    }

    bool ReadReviews(uint256 hash, vector<CReview>& vReviews);
    bool WriteReviews(uint256 hash, const vector<CReview>& vReviews);
};





class CMarketDB : public CDB
{
public:
    CMarketDB(const char* pszMode="r+", bool fTxn=false) : CDB("market.dat", pszMode, fTxn) { }
private:
    CMarketDB(const CMarketDB&);
    void operator=(const CMarketDB&);
};




// "CAddrDB" 是比特币核心客户端中的一个数据库，用于存储和管理网络节点的地址信息。
// 具体来说，它是用于管理比特币客户端中每个已知节点的元数据和索引信息的数据库。
// "CAddrDB" 包含了比特币网络中所有可用节点的IP地址、端口号、版本信息等详细信息。
// 当比特币客户端启动时，它会从"CAddrDB" 中获取可用节点的列表，
// 并使用这些节点进行数据同步和区块链验证。
// 使用"CAddrDB" 可以提高比特币客户端的性能，
// 并使其更容易访问和管理网络节点信息。通过快速查找和访问需要的节点数据，
// 可以加快比特币客户端与其他节点之间的连接速度和同步时间，提高比特币网络的整体效率。
class CAddrDB : public CDB
{
public:
    CAddrDB(const char* pszMode="r+", bool fTxn=false) : CDB("addr.dat", pszMode, fTxn) { }
private:
    CAddrDB(const CAddrDB&);
    void operator=(const CAddrDB&);
public:
    bool WriteAddress(const CAddress& addr);
    bool LoadAddresses();
};

bool LoadAddresses();





// "CWalletDB" 是比特币核心客户端中的一个数据库，
// 用于存储和管理比特币钱包的交易数据、账户信息、私钥等敏感信息。
// "CWalletDB" 包含了比特币客户端中所有交易和钱包账户的相关信息。
// 它可以存储比特币地址、私钥、余额、交易历史记录等数据，同时还提供了对这些数据的安全访问和管理功能。
class CWalletDB : public CDB
{
public:
    CWalletDB(const char* pszMode="r+", bool fTxn=false) : CDB("wallet.dat", pszMode, fTxn) { }
private:
    CWalletDB(const CWalletDB&);
    void operator=(const CWalletDB&);
public:
    bool ReadName(const string& strAddress, string& strName)
    {
        strName = "";
        return Read(make_pair(string("name"), strAddress), strName);
    }

    bool WriteName(const string& strAddress, const string& strName)
    {
        mapAddressBook[strAddress] = strName;
        return Write(make_pair(string("name"), strAddress), strName);
    }

    bool EraseName(const string& strAddress)
    {
        mapAddressBook.erase(strAddress);
        return Erase(make_pair(string("name"), strAddress));
    }

    bool ReadTx(uint256 hash, CWalletTx& wtx)
    {
        return Read(make_pair(string("tx"), hash), wtx);
    }

    bool WriteTx(uint256 hash, const CWalletTx& wtx)
    {
        return Write(make_pair(string("tx"), hash), wtx);
    }

    bool EraseTx(uint256 hash)
    {
        return Erase(make_pair(string("tx"), hash));
    }

    bool ReadKey(const vector<unsigned char>& vchPubKey, CPrivKey& vchPrivKey)
    {
        vchPrivKey.clear();
        return Read(make_pair(string("key"), vchPubKey), vchPrivKey);
    }

    bool WriteKey(const vector<unsigned char>& vchPubKey, const CPrivKey& vchPrivKey)
    {
        return Write(make_pair(string("key"), vchPubKey), vchPrivKey, false);
    }

    bool ReadDefaultKey(vector<unsigned char>& vchPubKey)
    {
        vchPubKey.clear();
        return Read(string("defaultkey"), vchPubKey);
    }

    bool WriteDefaultKey(const vector<unsigned char>& vchPubKey)
    {
        return Write(string("defaultkey"), vchPubKey);
    }

    template<typename T>
    bool ReadSetting(const string& strKey, T& value)
    {
        return Read(make_pair(string("setting"), strKey), value);
    }

    template<typename T>
    bool WriteSetting(const string& strKey, const T& value)
    {
        return Write(make_pair(string("setting"), strKey), value);
    }

    bool LoadWallet(vector<unsigned char>& vchDefaultKeyRet);
};

bool LoadWallet();

inline bool SetAddressBookName(const string& strAddress, const string& strName)
{
    return CWalletDB().WriteName(strAddress, strName);
}
