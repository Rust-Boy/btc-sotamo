// Copyright (c) 2009 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"






//
// CDB
//

// 实现多线程同步和互斥
// 在比特币协议中， CCriticalSection 通常用于保护与网络通信、磁盘读写等操作相关的数据结构和变量。
static CCriticalSection cs_db;
// 表示是否初始化数据库环境变量的标志位
static bool fDbEnvInit = false;
// 用于管理和维护数据库环境
// 用于处理与数据库相关的操作。
DbEnv dbenv(0);
// 用于表示内存映射文件的使用计数器。
// 在一些程序或库中，为了避免重复映射同一个文件导致资源浪费和性能问题，
// 会使用使用计数器来追踪内存映射文件被使用的次数。
static map<string, int> mapFileUseCount;

// 用于初始化和管理 Berkeley DB 数据库。Berkeley DB 库被用于存储比特币区块链数据和钱包相关数据
// CDBInit 类提供了数据库初始化、打开、关闭等操作的方法，并封装了 Berkeley DB 库的许多功能，
// 如环境变量设置、清理、备份等。它还实现了对内存池进行统计和限制的功能，以防止内存泄漏和性能问题。
// 在比特币协议中，CDBInit 类是比特币核心和其他钱包软件中的重要组件之一，用于处理与数据库相关的操作。
class CDBInit
{
public:
    CDBInit()
    {
    }
    ~CDBInit()
    {
        if (fDbEnvInit)
        {
            dbenv.close(0);
            fDbEnvInit = false;
        }
    }
}
// 用于检查给定对象是否为 CDBInit 类的实例。
// 因此，需要查看上下文来确定其确切含义。
// 常见的实现方式是使用动态类型识别（dynamic type identification）
// 或模板元编程（template metaprogramming）技术，在运行时或编译时检查对象类型并返回布尔值结果。
instance_of_cdbinit;


// CDB 类是一个封装了 Berkeley DB 库的数据库访问对象，
// 用于读写比特币区块链数据和钱包相关数据。该构造函数接受三个参数：
// 数据库文件名、打开模式和事务标志位。其中，数据库文件名指定数据库文件的路径和名称
// ，打开模式指定数据库打开方式，可以是 "r"（只读），"w"（可读可写）或其他有效的模式字符串，
// 事务标志位指定是否启用事务处理模式。
// 在构造函数中，首先将 pdb 成员初始化为 NULL，然后调用 Open 方法打开指定的数据库文件，
// 并以指定的打开模式打开数据库。如果数据库打开成功，则根据事务标志位决定是否启用事务处理模式
// 。如果启用事务，则调用 TxnBegin 方法开始事务处理；否则，跳过事务处理过程
// 。如果数据库打开失败，则抛出异常并设置 pdb 为 NULL。
CDB::CDB(const char* pszFile, const char* pszMode, bool fTxn) : pdb(NULL)
{
    int ret;
    if (pszFile == NULL)
        return;

    // 用于表示是否创建新文件的标志位。
    // 在一些程序或库中，为了避免覆盖或误删除已有文件
    // ，会使用 fCreate 标志位来指示是否创建新文件。
    bool fCreate = strchr(pszMode, 'c');
    bool fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));
    unsigned int nFlags = DB_THREAD;
    if (fCreate)
        nFlags |= DB_CREATE;
    else if (fReadOnly)
        nFlags |= DB_RDONLY;
    if (!fReadOnly || fTxn)
        nFlags |= DB_AUTO_COMMIT;

    CRITICAL_BLOCK(cs_db)
    {
        if (!fDbEnvInit)
        {
            string strAppDir = GetAppDir();
            string strLogDir = strAppDir + "\\database";
            _mkdir(strLogDir.c_str());
            printf("dbenv.open strAppDir=%s\n", strAppDir.c_str());

            dbenv.set_lg_dir(strLogDir.c_str());
            dbenv.set_lg_max(10000000);
            dbenv.set_lk_max_locks(10000);
            dbenv.set_lk_max_objects(10000);
            dbenv.set_errfile(fopen("db.log", "a")); /// debug
            ///dbenv.log_set_config(DB_LOG_AUTO_REMOVE, 1); /// causes corruption
            ret = dbenv.open(strAppDir.c_str(),
                             DB_CREATE     |
                             DB_INIT_LOCK  |
                             DB_INIT_LOG   |
                             DB_INIT_MPOOL |
                             DB_INIT_TXN   |
                             DB_THREAD     |
                             DB_PRIVATE    |
                             DB_RECOVER,
                             0);
            if (ret > 0)
                throw runtime_error(strprintf("CDB() : error %d opening database environment\n", ret));
            fDbEnvInit = true;
        }

        strFile = pszFile;
        ++mapFileUseCount[strFile];
    }

    pdb = new Db(&dbenv, 0);

    ret = pdb->open(NULL,      // Txn pointer
                    pszFile,   // Filename
                    "main",    // Logical db name
                    DB_BTREE,  // Database type
                    nFlags,    // Flags
                    0);

    if (ret > 0)
    {
        delete pdb;
        pdb = NULL;
        CRITICAL_BLOCK(cs_db)
            --mapFileUseCount[strFile];
        strFile = "";
        throw runtime_error(strprintf("CDB() : can't open database file %s, error %d\n", pszFile, ret));
    }

    if (fCreate && !Exists(string("version")))
        WriteVersion(VERSION);

    // 用于为随机数生成器添加额外种子值的函数或方法
    RandAddSeed();
}

// 关闭数据库连接的方法或函数。这可以确保该连接分配的任何资源或内存都被释放
void CDB::Close()
{
    if (!pdb)
        return;
    if (!vTxn.empty())
        vTxn.front()->abort();
    vTxn.clear();
    pdb->close(0);
    delete pdb;
    pdb = NULL;
    dbenv.txn_checkpoint(0, 0, 0);

    CRITICAL_BLOCK(cs_db)
        --mapFileUseCount[strFile];

    RandAddSeed();
}

// 将待处理的数据库写入刷新到磁盘的方法或函数
void DBFlush(bool fShutdown)
{
    // Flush log data to the actual data file
    //  on all files that are not in use
    printf("DBFlush(%s)\n", fShutdown ? "true" : "false");
    CRITICAL_BLOCK(cs_db)
    {
        dbenv.txn_checkpoint(0, 0, 0);
        map<string, int>::iterator mi = mapFileUseCount.begin();
        while (mi != mapFileUseCount.end())
        {
            string strFile = (*mi).first;
            int nRefCount = (*mi).second;
            if (nRefCount == 0)
            {
                dbenv.lsn_reset(strFile.c_str(), 0);
                mapFileUseCount.erase(mi++);
            }
            else
                mi++;
        }
        if (fShutdown)
        {
            char** listp;
            if (mapFileUseCount.empty())
                dbenv.log_archive(&listp, DB_ARCH_REMOVE);
            dbenv.close(0);
            fDbEnvInit = false;
        }
    }
}






//
// CTxDB
//
//
bool CTxDB::ReadTxIndex(uint256 hash, CTxIndex& txindex)
{
    assert(!fClient);
    txindex.SetNull();
    return Read(make_pair(string("tx"), hash), txindex);
}


// 用于更新事务索引的方法或函数。事务索引通常用于数据库中跟踪挂起的事务及其关联的数据更改。
// "UpdateTxIndex"可以用于更新事务的状态，以及由该事务进行的任何相关数据修改。
bool CTxDB::UpdateTxIndex(uint256 hash, const CTxIndex& txindex)
{
    assert(!fClient);
    return Write(make_pair(string("tx"), hash), txindex);
}


// 用于向事务索引添加新事务的方法或函数。
// 事务索引通常用于数据库中跟踪挂起的事务及其关联的数据更改。
// "AddTxIndex"可以用于在索引中创建一个新的事务记录，
// 然后可以使用该记录来跟踪事务的状态和其关联的数据修改。
bool CTxDB::AddTxIndex(const CTransaction& tx, const CDiskTxPos& pos, int nHeight)
{
    assert(!fClient);

    // Add to tx index
    uint256 hash = tx.GetHash();
    CTxIndex txindex(pos, tx.vout.size());
    return Write(make_pair(string("tx"), hash), txindex);
}

bool CTxDB::EraseTxIndex(const CTransaction& tx)
{
    assert(!fClient);
    uint256 hash = tx.GetHash();

    return Erase(make_pair(string("tx"), hash));
}

// 用于检查某个区块是否包含特定交易的哈希值。
// 该函数接受一个区块和一个交易哈希值作为参数，
// 并返回布尔值表示区块是否包含该交易。
// 这个函数通常用于验证某个交易是否已经被打包进一个区块中。
bool CTxDB::ContainsTx(uint256 hash)
{
    assert(!fClient);
    return Exists(make_pair(string("tx"), hash));
}

// 为读取某个钱包地址的所有交易记录
bool CTxDB::ReadOwnerTxes(uint160 hash160, int nMinHeight, vector<CTransaction>& vtx)
{
    assert(!fClient);
    vtx.clear();

    // Get cursor
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;

    unsigned int fFlags = DB_SET_RANGE;
    loop
    {
        // Read next record
        CDataStream ssKey;
        if (fFlags == DB_SET_RANGE)
            ssKey << string("owner") << hash160 << CDiskTxPos(0, 0, 0);
        CDataStream ssValue;
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
            return false;

        // Unserialize
        string strType;
        uint160 hashItem;
        CDiskTxPos pos;
        ssKey >> strType >> hashItem >> pos;
        int nItemHeight;
        ssValue >> nItemHeight;

        // Read transaction
        if (strType != "owner" || hashItem != hash160)
            break;
        if (nItemHeight >= nMinHeight)
        {
            vtx.resize(vtx.size()+1);
            if (!vtx.back().ReadFromDisk(pos))
                return false;
        }
    }
    return true;
}

// 从磁盘上的区块文件中读取指定交易的原始数据。
// 该函数接受一个交易哈希值作为参数，并返回一个包含交易原始字节流的对象。
// 这个函数通常被调用来获取尚未进入内存池的交易的数据，以便进行进一步处理或验证。
bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx, CTxIndex& txindex)
{
    assert(!fClient);
    tx.SetNull();
    if (!ReadTxIndex(hash, txindex))
        return false;
    return (tx.ReadFromDisk(txindex.pos));
}

bool CTxDB::ReadDiskTx(uint256 hash, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx, CTxIndex& txindex)
{
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

bool CTxDB::ReadDiskTx(COutPoint outpoint, CTransaction& tx)
{
    CTxIndex txindex;
    return ReadDiskTx(outpoint.hash, tx, txindex);
}

// 用于将块索引数据写入磁盘。该函数接受一个块索引对象作为参数，
// 并将其序列化后写入到指定的块索引文件中。
// 块索引是比特币节点中用于跟踪所有已知块的数据结构，
// 包括每个块的元信息（如高度、哈希值等），以及它们之间的关系
// 。这些数据对于区块链同步和查询非常重要。
bool CTxDB::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    return Write(make_pair(string("blockindex"), blockindex.GetBlockHash()), blockindex);
}

// 用于从内存中删除指定的块索引对象。该函数接受一个块哈希值作为参数，
// 并查找相应的块索引对象，如果找到则将其从内存中删除。
// 块索引是比特币节点中用于跟踪所有已知块的数据结构，
// 包括每个块的元信息（如高度、哈希值等），以及它们之间的关系。
// 这些数据对于区块链同步和查询非常重要。
bool CTxDB::EraseBlockIndex(uint256 hash)
{
    return Erase(make_pair(string("blockindex"), hash));
}

// 读取当前节点认为是最佳链的顶部区块的哈希值
bool CTxDB::ReadHashBestChain(uint256& hashBestChain)
{
    return Read(string("hashBestChain"), hashBestChain);
}

// 将当前节点认为是最佳链的顶部区块的哈希值写入磁盘
bool CTxDB::WriteHashBestChain(uint256 hashBestChain)
{
    return Write(string("hashBestChain"), hashBestChain);
}

// 用于将新的块索引对象插入到内存中。
// 该函数接受一个块索引对象作为参数，并将其插入到内存中的块索引数据结构中，
// 以便在接收到新的块数据时进行处理和验证。块索引是比特币节点中用于跟踪所有已知块的数据结构，
// 包括每个块的元信息（如高度、哈希值等）
CBlockIndex* InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
        return NULL;

    // Return existing
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    // Create new
    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

// 从磁盘上的块索引文件中加载块索引数据。
// 该函数在比特币节点启动时被调用，
// 并将磁盘上已保存的块索引数据加载到内存中。
// 块索引是比特币节点中用于跟踪所有已知块的数据结构，
// 包括每个块的元信息（如高度、哈希值等），以及它们之间的关系
bool CTxDB::LoadBlockIndex()
{
    // Get cursor
    Dbc* pcursor = GetCursor();
    if (!pcursor)
        return false;

    unsigned int fFlags = DB_SET_RANGE;
    loop
    {
        // Read next record
        CDataStream ssKey;
        if (fFlags == DB_SET_RANGE)
            ssKey << make_pair(string("blockindex"), uint256(0));
        CDataStream ssValue;
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
            return false;

        // Unserialize
        string strType;
        ssKey >> strType;
        if (strType == "blockindex")
        {
            CDiskBlockIndex diskindex;
            ssValue >> diskindex;

            // Construct block index object
            CBlockIndex* pindexNew = InsertBlockIndex(diskindex.GetBlockHash());
            pindexNew->pprev          = InsertBlockIndex(diskindex.hashPrev);
            pindexNew->pnext          = InsertBlockIndex(diskindex.hashNext);
            pindexNew->nFile          = diskindex.nFile;
            pindexNew->nBlockPos      = diskindex.nBlockPos;
            pindexNew->nHeight        = diskindex.nHeight;
            pindexNew->nVersion       = diskindex.nVersion;
            pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
            pindexNew->nTime          = diskindex.nTime;
            pindexNew->nBits          = diskindex.nBits;
            pindexNew->nNonce         = diskindex.nNonce;

            // Watch for genesis block and best block
            if (pindexGenesisBlock == NULL && diskindex.GetBlockHash() == hashGenesisBlock)
                pindexGenesisBlock = pindexNew;
        }
        else
        {
            break;
        }
    }

    if (!ReadHashBestChain(hashBestChain))
    {
        if (pindexGenesisBlock == NULL)
            return true;
        return error("CTxDB::LoadBlockIndex() : hashBestChain not found\n");
    }

    if (!mapBlockIndex.count(hashBestChain))
        return error("CTxDB::LoadBlockIndex() : blockindex for hashBestChain not found\n");
    pindexBest = mapBlockIndex[hashBestChain];
    nBestHeight = pindexBest->nHeight;
    printf("LoadBlockIndex(): hashBestChain=%s  height=%d\n", hashBestChain.ToString().substr(0,14).c_str(), nBestHeight);

    return true;
}





//
// CAddrDB
//

// 用于管理节点保存的地址信息数据库。
// 该类提供了将地址信息（如 IP 地址、端口等）写入磁盘和从磁盘读取地址信息的功能
// ，以及对地址信息进行索引和查询的功能。节点使用 CAddrDB 来存储已知的网络节点地址
// ，以便在需要时连接到其他节点或广播自己的地址。
// 将地址信息写入磁盘的功能
bool CAddrDB::WriteAddress(const CAddress& addr)
{
    // CAddress用于表示网络节点的地址信息，
    // 包括 IP 地址、端口号、服务标识等。
    // 该类提供了对地址信息进行解析、序列化和比较的功能，
    // 并可以与其他节点交换地址信息以便建立连接。
    // CAddress 对象通常被存储在 CAddrDB 数据库中，
    // 以便节点在需要时动态地更新和查询网络中其他节点的地址信息。
    return Write(make_pair(string("addr"), addr.GetKey()), addr);
}

// 从地址信息数据库中加载所有已知的网络节点地址。
// 该函数会读取数据库中存储的所有地址信息，并将其解析为 CAddress 对象的形式，
// 然后返回一个包含所有地址信息的向量。这些地址信息可以用于连接其他网络节点或广播自己的地址信息。
bool CAddrDB::LoadAddresses()
{
    // 用于将一段代码标记为关键代码块（Critical Block）。
    // 该宏定义接受一个互斥量对象作为参数，并在代码块执行期间对该互斥量进行加锁操作，
    // 以确保多线程环境下的同步和互斥。在比特币节点中，由于涉及到大量共享数据结构的读写操作，
    // 使用 CRITICAL_BLOCK 宏定义来保证数据同步性是非常常见的。
    CRITICAL_BLOCK(cs_mapIRCAddresses)
    CRITICAL_BLOCK(cs_mapAddresses)
    {
        // Load user provided addresses
        // 用于简化文件读写操作。该类封装了文件操作相关的底层细节，
        // 提供了一组方便的函数接口，以便进行文件打开、关闭、读取和写入等操作。
        // 此外，CAutoFile 还实现了自动资源管理，即在对象生命周期结束时会自动释放底层资源，
        // 从而避免了常见的内存泄漏和资源泄漏问题。
        // 在比特币节点中，CAutoFile 类通常用于读取和写入区块和交易数据等重要数据。
        CAutoFile filein = fopen("addr.txt", "rt");
        if (filein)
        {
            try
            {
                char psz[1000];
                while (fgets(psz, sizeof(psz), filein))
                {
                    CAddress addr(psz, NODE_NETWORK);
                    if (addr.ip != 0)
                    {
                        AddAddress(*this, addr);
                        mapIRCAddresses.insert(make_pair(addr.GetKey(), addr));
                    }
                }
            }
            catch (...) { }
        }

        // Get cursor
        // 游标
        Dbc* pcursor = GetCursor();
        if (!pcursor)
            return false;

        loop
        {
            // Read next record
            // CDataStream用于进行二进制数据流的序列化和反序列化
            // 可以将各种数据类型（如整数、字符串、数组、结构体等）
            // 序列化为字节流或者将字节流反序列化为具体的数据类型。
            // CDataStream 类在比特币节点中被广泛使用，包括对区块和交易数据进行编码和解码，
            // 以及网络消息的发送和接收等方面。
            CDataStream ssKey;
            CDataStream ssValue;
            int ret = ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0)
                return false;

            // Unserialize
            string strType;
            ssKey >> strType;
            if (strType == "addr")
            {
                CAddress addr;
                ssValue >> addr;
                mapAddresses.insert(make_pair(addr.GetKey(), addr));
            }
        }

        //// debug print
        printf("mapAddresses:\n");
        // 在比特币源代码中，PAIRTYPE 可能是某些数据结构（如哈希表、字典等）
        // 中使用的泛型类型名称，以便支持不同类型的键值对操作。
        foreach(const PAIRTYPE(vector<unsigned char>, CAddress)& item, mapAddresses)
            item.second.print();
        printf("-----\n");

        // Fix for possible bug that manifests in mapAddresses.count in irc.cpp,
        // just need to call count here and it doesn't happen there.  The bug was the
        // pack pragma in irc.cpp and has been fixed, but I'm not in a hurry to delete this.
        // 用于返回指定键值在 map 容器中出现的次数
        // 在比特币节点中，mapAddresses 是一个存储网络节点地址信息的 std::map 容器，
        // 用于记录已知的网络节点地址和相应的服务标识等信息。
        // 因此，"mapAddresses.count" 
        // 可能被用于查询某个网络节点地址是否已经存在于 mapAddresses 中。
        mapAddresses.count(vector<unsigned char>(18));
    }

    return true;
}

bool LoadAddresses()
{
    return CAddrDB("cr+").LoadAddresses();
}




//
// CReviewDB
//
// 可能为从某个数据存储区域（如数据库、文件等）读取评价信息
bool CReviewDB::ReadReviews(uint256 hash, vector<CReview>& vReviews)
{
    vReviews.size(); // msvc workaround, just need to do anything with vReviews
    return Read(make_pair(string("reviews"), hash), vReviews);
}

bool CReviewDB::WriteReviews(uint256 hash, const vector<CReview>& vReviews)
{
    // "make_pair" 是 C++ STL 中的一个模板函数
    // ，用于创建一个 std::pair 对象。该函数接受两个参数，
    // 并将它们封装成一个有序对返回。std::pair 表示一对键值对，
    // 可以通过 pair.first 和 pair.second 访问其两个元素。
    // 在比特币源代码中，make_pair 
    // 可能被用于创建一些数据结构（如 map、vector 等）中的键值对。
    return Write(make_pair(string("reviews"), hash), vReviews);
}







//
// CWalletDB
//
// 用于管理钱包相关的数据存储和操作。该类封装了 Berkeley DB 数据库，
// 提供了一组方便的函数接口，可以将钱包相关的交易、地址、私钥等信息写入磁盘，
// 以及从磁盘读取这些信息并进行查询、更新等操作。
// CWalletDB 在比特币节点中扮演着重要的角色，
// 用于保存和管理用户的钱包信息，包括账户余额、交易历史、收发地址等。
bool CWalletDB::LoadWallet(vector<unsigned char>& vchDefaultKeyRet)
{
    // 于加载钱包相关的数据。
    // 该函数在比特币节点启动时被调用，并从磁盘上的钱包文件中读取用户的钱包信息，
    // 例如账户余额、交易历史、收发地址等，并将这些信息存储到内存中，
    // 以便进行快速查询和更新。在比特币节点运行期间，
    // 用户可以通过执行类似 "sendtoaddress"、 "getbalance" 等命令来与自己的钱包进行交互，
    // 而 LoadWallet 函数则是提供这些功能的基础。
    vchDefaultKeyRet.clear();

    //// todo: shouldn't we catch exceptions and try to recover and continue?
    CRITICAL_BLOCK(cs_mapKeys)
    CRITICAL_BLOCK(cs_mapWallet)
    {
        // Get cursor
        Dbc* pcursor = GetCursor();
        if (!pcursor)
            return false;

        loop
        {
            // Read next record
            CDataStream ssKey;
            CDataStream ssValue;
            int ret = ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0)
                return false;

            // Unserialize
            // Taking advantage of the fact that pair serialization
            // is just the two items serialized one after the other
            string strType;
            ssKey >> strType;
            if (strType == "name")
            {
                string strAddress;
                ssKey >> strAddress;
                ssValue >> mapAddressBook[strAddress];
            }
            else if (strType == "tx")
            {
                uint256 hash;
                ssKey >> hash;
                CWalletTx& wtx = mapWallet[hash];
                ssValue >> wtx;

                if (wtx.GetHash() != hash)
                    printf("Error in wallet.dat, hash mismatch\n");

                //// debug print
                //printf("LoadWallet  %s\n", wtx.GetHash().ToString().c_str());
                //printf(" %12I64d  %s  %s  %s\n",
                //    wtx.vout[0].nValue,
                //    DateTimeStr(wtx.nTime).c_str(),
                //    wtx.hashBlock.ToString().substr(0,14).c_str(),
                //    wtx.mapValue["message"].c_str());
            }
            else if (strType == "key")
            {
                vector<unsigned char> vchPubKey;
                ssKey >> vchPubKey;
                CPrivKey vchPrivKey;
                ssValue >> vchPrivKey;

                mapKeys[vchPubKey] = vchPrivKey;
                mapPubKeys[Hash160(vchPubKey)] = vchPubKey;
            }
            else if (strType == "defaultkey")
            {
                ssValue >> vchDefaultKeyRet;
            }
            else if (strType == "setting")  /// or settings or option or options or config?
            {
                string strKey;
                ssKey >> strKey;
                if (strKey == "fGenerateBitcoins")  ssValue >> fGenerateBitcoins;
                if (strKey == "nTransactionFee")    ssValue >> nTransactionFee;
                if (strKey == "addrIncoming")       ssValue >> addrIncoming;
            }
        }
    }

    printf("fGenerateBitcoins = %d\n", fGenerateBitcoins);
    printf("nTransactionFee = %I64d\n", nTransactionFee);
    printf("addrIncoming = %s\n", addrIncoming.ToString().c_str());

    return true;
}

bool LoadWallet()
{
    vector<unsigned char> vchDefaultKey;
    if (!CWalletDB("cr").LoadWallet(vchDefaultKey))
        return false;

    if (mapKeys.count(vchDefaultKey))
    {
        // Set keyUser
        keyUser.SetPubKey(vchDefaultKey);
        keyUser.SetPrivKey(mapKeys[vchDefaultKey]);
    }
    else
    {
        // Create new keyUser and set as default key
        // 用于增加随机数生成器的种子。该函数接受一个整数值作为参数，
        // 并将其与当前的随机数种子进行异或运算，从而增加随机数生成器的种子。
        // 在比特币节点中，RandAddSeed 函数通常被用于增强密码学安全性，
        // 例如在创建新的密钥对、签名交易等过程中使用。通过增加随机数种子，
        // 可以增加密钥和签名的随机性，从而提高系统的安全性。
        RandAddSeed(true);
        keyUser.MakeNewKey();
        if (!AddKey(keyUser))
            return false;
        if (!SetAddressBookName(PubKeyToAddress(keyUser.GetPubKey()), "Your Address"))
            return false;
        // 用于生成并写入默认密钥（Default Key）。
        // 该函数在比特币节点启动时被调用，并生成一个新的 ECDSA 密钥对（公钥和私钥），
        // 然后将其存储到钱包数据库中。默认密钥通常用于接收用户的找零交易，
        // 即将多余的比特币转回自己的地址中。在比特币节点运行期间，
        // 用户可以使用类似 "getnewaddress"、"dumpprivkey" 等命令来管理自己的密钥对
        // ，而 WriteDefaultKey 函数则是提供这些功能的基础。
        CWalletDB().WriteDefaultKey(keyUser.GetPubKey());
    }

    return true;
}
