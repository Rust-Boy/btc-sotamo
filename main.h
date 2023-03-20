// Copyright (c) 2009 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

class COutPoint;
class CInPoint;
class CDiskTxPos;
class CCoinBase;
class CTxIn;
class CTxOut;
class CTransaction;
class CBlock;
class CBlockIndex;
class CWalletTx;
class CKeyItem;

static const unsigned int MAX_SIZE = 0x02000000;
static const int64 COIN = 100000000;
static const int64 CENT = 1000000;
static const int COINBASE_MATURITY = 100;

static const CBigNum bnProofOfWorkLimit(~uint256(0) >> 32);






extern CCriticalSection cs_main;
extern map<uint256, CBlockIndex*> mapBlockIndex;
extern const uint256 hashGenesisBlock;
extern CBlockIndex* pindexGenesisBlock;
extern int nBestHeight;
extern uint256 hashBestChain;
extern CBlockIndex* pindexBest;
extern unsigned int nTransactionsUpdated;
extern string strSetDataDir;
extern int nDropMessagesTest;

// Settings
extern int fGenerateBitcoins;
extern int64 nTransactionFee;
extern CAddress addrIncoming;







// 用于确定钱包文件的默认存储位置，以及其他相关文件（如日志文件、锁文件等）的存储位置
string GetAppDir();
// 检查钱包文件存储路径下的磁盘空间是否充足，以便在需要时可以保存新的交易数据和其他相关信息
bool CheckDiskSpace(int64 nAdditionalBytes=0);
// 打开指定区块文件（block file），以便读取或写入该文件中的数据
FILE* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode="rb");
// 将新的区块数据追加到指定的区块文件（block file）中
FILE* AppendBlockFile(unsigned int& nFileRet);
bool AddKey(const CKey& key);
vector<unsigned char> GenerateNewKey();
// 将一个新的交易输出（transaction output）添加到钱包（wallet）中
bool AddToWallet(const CWalletTx& wtxIn);
// 将钱包（Wallet）中未确认的交易重新提交到交易池（Transaction pool）中
void ReacceptWalletTransactions();
// 是将钱包（Wallet）中的交易广播到比特币网络中
void RelayWalletTransactions();
// 是从本地磁盘上的区块文件中加载和建立区块索引（block index）
bool LoadBlockIndex(bool fAllowNew=true);
// 打印当前节点内存中的区块链（Blockchain）结构
void PrintBlockTree();
bool BitcoinMiner();
bool ProcessMessages(CNode* pfrom);
// 处理来自比特币网络的消息
bool ProcessMessage(CNode* pfrom, string strCommand, CDataStream& vRecv);
// 将消息发送到比特币网络中的其他节点
bool SendMessages(CNode* pto);
int64 GetBalance();
// 创建一笔新的比特币交易
bool CreateTransaction(CScript scriptPubKey, int64 nValue, CWalletTx& txNew, int64& nFeeRequiredRet);
// 将指定交易的输出标记为已经花费（spent）
bool CommitTransactionSpent(const CWalletTx& wtxNew);
// 尝试向某个地址或账户发送一定数量的资金
bool SendMoney(CScript scriptPubKey, int64 nValue, CWalletTx& wtxNew);










// 表示某个区块文件中的交易记录的位置
class CDiskTxPos
{
public:
    unsigned int nFile;
    unsigned int nBlockPos;
    unsigned int nTxPos;

    CDiskTxPos()
    {
        SetNull();
    }

    CDiskTxPos(unsigned int nFileIn, unsigned int nBlockPosIn, unsigned int nTxPosIn)
    {
        nFile = nFileIn;
        nBlockPos = nBlockPosIn;
        nTxPos = nTxPosIn;
    }

    IMPLEMENT_SERIALIZE( READWRITE(FLATDATA(*this)); )
    void SetNull() { nFile = -1; nBlockPos = 0; nTxPos = 0; }
    bool IsNull() const { return (nFile == -1); }

    friend bool operator==(const CDiskTxPos& a, const CDiskTxPos& b)
    {
        return (a.nFile     == b.nFile &&
                a.nBlockPos == b.nBlockPos &&
                a.nTxPos    == b.nTxPos);
    }

    friend bool operator!=(const CDiskTxPos& a, const CDiskTxPos& b)
    {
        return !(a == b);
    }

    string ToString() const
    {
        if (IsNull())
            return strprintf("null");
        else
            return strprintf("(nFile=%d, nBlockPos=%d, nTxPos=%d)", nFile, nBlockPos, nTxPos);
    }

    void print() const
    {
        printf("%s", ToString().c_str());
    }
};




// 表示一笔交易输入（Transaction Input）所引用的上一笔交易输出
class CInPoint
{
public:
// 上一笔交易输出的位置信息，包括上一笔交易的哈希值和输出索引
    CTransaction* ptx;
    unsigned int n;

    CInPoint() { SetNull(); }
    CInPoint(CTransaction* ptxIn, unsigned int nIn) { ptx = ptxIn; n = nIn; }
    void SetNull() { ptx = NULL; n = -1; }
    bool IsNull() const { return (ptx == NULL && n == -1); }
};




// 表示一笔交易输出（Transaction Output）在交易池（mempool）或区块链中的位置
class COutPoint
{
public:
    // 表示上一笔交易的哈希值
    uint256 hash;
    // 表示上一笔交易的输出索引
    unsigned int n;

    COutPoint() { SetNull(); }
    COutPoint(uint256 hashIn, unsigned int nIn) { hash = hashIn; n = nIn; }
    IMPLEMENT_SERIALIZE( READWRITE(FLATDATA(*this)); )
    void SetNull() { hash = 0; n = -1; }
    bool IsNull() const { return (hash == 0 && n == -1); }

    friend bool operator<(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash < b.hash || (a.hash == b.hash && a.n < b.n));
    }

    friend bool operator==(const COutPoint& a, const COutPoint& b)
    {
        return (a.hash == b.hash && a.n == b.n);
    }

    friend bool operator!=(const COutPoint& a, const COutPoint& b)
    {
        return !(a == b);
    }

    string ToString() const
    {
        return strprintf("COutPoint(%s, %d)", hash.ToString().substr(0,6).c_str(), n);
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};




//
// An input of a transaction.  It contains the location of the previous
// transaction's output that it claims and a signature that matches the
// output's public key.
//
// 表示一笔交易输入（Transaction Input）
class CTxIn
{
public:
    // 表示被引用的上一笔交易输出的位置信息，包括上一笔交易的哈希值和输出索引
    COutPoint prevout;
    // 表示解锁脚本，用于验证该交易输入是否有权花费上一笔交易输出的资产
    CScript scriptSig;
    // 表示交易版本号
    unsigned int nSequence;

    CTxIn()
    {
        nSequence = UINT_MAX;
    }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), unsigned int nSequenceIn=UINT_MAX)
    {
        prevout = prevoutIn;
        scriptSig = scriptSigIn;
        nSequence = nSequenceIn;
    }

    CTxIn(uint256 hashPrevTx, unsigned int nOut, CScript scriptSigIn=CScript(), unsigned int nSequenceIn=UINT_MAX)
    {
        prevout = COutPoint(hashPrevTx, nOut);
        scriptSig = scriptSigIn;
        nSequence = nSequenceIn;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(prevout);
        READWRITE(scriptSig);
        READWRITE(nSequence);
    )

    bool IsFinal() const
    {
        return (nSequence == UINT_MAX);
    }

    friend bool operator==(const CTxIn& a, const CTxIn& b)
    {
        return (a.prevout   == b.prevout &&
                a.scriptSig == b.scriptSig &&
                a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn& a, const CTxIn& b)
    {
        return !(a == b);
    }

    string ToString() const
    {
        string str;
        str += strprintf("CTxIn(");
        str += prevout.ToString();
        if (prevout.IsNull())
            str += strprintf(", coinbase %s", HexStr(scriptSig.begin(), scriptSig.end(), false).c_str());
        else
            str += strprintf(", scriptSig=%s", scriptSig.ToString().substr(0,24).c_str());
        if (nSequence != UINT_MAX)
            str += strprintf(", nSequence=%u", nSequence);
        str += ")";
        return str;
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }

    bool IsMine() const;
    int64 GetDebit() const;
};




//
// An output of a transaction.  It contains the public key that the next input
// must be able to sign with to claim it.
//
// 表示一笔交易输出（Transaction Output）
class CTxOut
{
public:
    // 表示输出金额，即该交易输出所包含的比特币数量
    int64 nValue;
    // 表示锁定脚本，用于控制该交易输出资产的使用权限
    CScript scriptPubKey;

public:
    CTxOut()
    {
        SetNull();
    }

    CTxOut(int64 nValueIn, CScript scriptPubKeyIn)
    {
        nValue = nValueIn;
        scriptPubKey = scriptPubKeyIn;
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(nValue);
        READWRITE(scriptPubKey);
    )

    void SetNull()
    {
        nValue = -1;
        scriptPubKey.clear();
    }

    bool IsNull()
    {
        return (nValue == -1);
    }

    // 计算某个对象（如交易、区块等）的哈希值
    uint256 GetHash() const
    {
        return SerializeHash(*this);
    }

    bool IsMine() const
    {
        return ::IsMine(scriptPubKey);
    }

    int64 GetCredit() const
    {
        if (IsMine())
            return nValue;
        return 0;
    }

    friend bool operator==(const CTxOut& a, const CTxOut& b)
    {
        return (a.nValue       == b.nValue &&
                a.scriptPubKey == b.scriptPubKey);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b)
    {
        return !(a == b);
    }

    string ToString() const
    {
        if (scriptPubKey.size() < 6)
            return "CTxOut(error)";
        return strprintf("CTxOut(nValue=%I64d.%08I64d, scriptPubKey=%s)", nValue / COIN, nValue % COIN, scriptPubKey.ToString().substr(0,24).c_str());
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};




//
// The basic transaction that is broadcasted on the network and contained in
// blocks.  A transaction can contain multiple inputs and outputs.
//
// 表示一笔比特币交易
class CTransaction
{
public:
    // 表示交易版本号
    int nVersion;
    // 表示交易输入（Transaction Input）列表
    vector<CTxIn> vin;
    // 表示交易输出（Transaction Output）列表
    vector<CTxOut> vout;
    // 表示交易锁定时间
    int nLockTime;


    CTransaction()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(vin);
        READWRITE(vout);
        READWRITE(nLockTime);
    )

    void SetNull()
    {
        nVersion = 1;
        vin.clear();
        vout.clear();
        nLockTime = 0;
    }

    bool IsNull() const
    {
        return (vin.empty() && vout.empty());
    }

    uint256 GetHash() const
    {
        return SerializeHash(*this);
    }

    // 于判断一笔交易是否已经达到最终状态
    bool IsFinal() const
    {
        if (nLockTime == 0 || nLockTime < nBestHeight)
            return true;
        foreach(const CTxIn& txin, vin)
            if (!txin.IsFinal())
                return false;
        return true;
    }

    // 判断某个交易是否比另一个交易更加新
    bool IsNewerThan(const CTransaction& old) const
    {
        if (vin.size() != old.vin.size())
            return false;
        for (int i = 0; i < vin.size(); i++)
            if (vin[i].prevout != old.vin[i].prevout)
                return false;

        bool fNewer = false;
        unsigned int nLowest = UINT_MAX;
        for (int i = 0; i < vin.size(); i++)
        {
            if (vin[i].nSequence != old.vin[i].nSequence)
            {
                if (vin[i].nSequence <= nLowest)
                {
                    fNewer = false;
                    nLowest = vin[i].nSequence;
                }
                if (old.vin[i].nSequence < nLowest)
                {
                    fNewer = true;
                    nLowest = old.vin[i].nSequence;
                }
            }
        }
        return fNewer;
    }

    // 判断一笔交易是否为挖矿奖励交易（Coinbase Transaction）
    bool IsCoinBase() const
    {
        return (vin.size() == 1 && vin[0].prevout.IsNull());
    }

    // 检查一笔交易是否合法。在比特币网络中，每笔交易都需要满足一系列规则和条件才能被确认和广播到网络中。
    bool CheckTransaction() const
    {
        // Basic checks that don't depend on any context
        if (vin.empty() || vout.empty())
            return error("CTransaction::CheckTransaction() : vin or vout empty");

        // Check for negative values
        foreach(const CTxOut& txout, vout)
            if (txout.nValue < 0)
                return error("CTransaction::CheckTransaction() : txout.nValue negative");

        if (IsCoinBase())
        {
            if (vin[0].scriptSig.size() < 2 || vin[0].scriptSig.size() > 100)
                return error("CTransaction::CheckTransaction() : coinbase script size");
        }
        else
        {
            foreach(const CTxIn& txin, vin)
                if (txin.prevout.IsNull())
                    return error("CTransaction::CheckTransaction() : prevout is null");
        }

        return true;
    }

    // 判断某个交易输出是否属于本地钱包地址（Wallet Address
    bool IsMine() const
    {
        foreach(const CTxOut& txout, vout)
            if (txout.IsMine())
                return true;
        return false;
    }

    int64 GetDebit() const
    {
        int64 nDebit = 0;
        foreach(const CTxIn& txin, vin)
            nDebit += txin.GetDebit();
        return nDebit;
    }

    int64 GetCredit() const
    {
        int64 nCredit = 0;
        foreach(const CTxOut& txout, vout)
            nCredit += txout.GetCredit();
        return nCredit;
    }

    // 计算一笔交易的总输出金额（Value Out）
    int64 GetValueOut() const
    {
        int64 nValueOut = 0;
        foreach(const CTxOut& txout, vout)
        {
            if (txout.nValue < 0)
                throw runtime_error("CTransaction::GetValueOut() : negative value");
            nValueOut += txout.nValue;
        }
        return nValueOut;
    }

    int64 GetMinFee(bool fDiscount=false) const
    {
        // Base fee is 1 cent per kilobyte
        unsigned int nBytes = ::GetSerializeSize(*this, SER_NETWORK);
        int64 nMinFee = (1 + (int64)nBytes / 1000) * CENT;

        // First 100 transactions in a block are free
        if (fDiscount && nBytes < 10000)
            nMinFee = 0;

        // To limit dust spam, require a 0.01 fee if any output is less than 0.01
        if (nMinFee < CENT)
            foreach(const CTxOut& txout, vout)
                if (txout.nValue < CENT)
                    nMinFee = CENT;

        return nMinFee;
    }


    // 从磁盘中读取区块链数据或其他数据结构。
    // 在比特币节点运行过程中，各种数据结构需要不断地进行读写操作
    // ，以支持区块同步、交易处理、钱包管理等功能。
    bool ReadFromDisk(CDiskTxPos pos, FILE** pfileRet=NULL)
    {
        CAutoFile filein = OpenBlockFile(pos.nFile, 0, pfileRet ? "rb+" : "rb");
        if (!filein)
            return error("CTransaction::ReadFromDisk() : OpenBlockFile failed");

        // Read transaction
        if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
            return error("CTransaction::ReadFromDisk() : fseek failed");
        filein >> *this;

        // Return file pointer
        if (pfileRet)
        {
            if (fseek(filein, pos.nTxPos, SEEK_SET) != 0)
                return error("CTransaction::ReadFromDisk() : second fseek failed");
            *pfileRet = filein.release();
        }
        return true;
    }


    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return (a.nVersion  == b.nVersion &&
                a.vin       == b.vin &&
                a.vout      == b.vout &&
                a.nLockTime == b.nLockTime);
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b)
    {
        return !(a == b);
    }


    string ToString() const
    {
        string str;
        str += strprintf("CTransaction(hash=%s, ver=%d, vin.size=%d, vout.size=%d, nLockTime=%d)\n",
            GetHash().ToString().substr(0,6).c_str(),
            nVersion,
            vin.size(),
            vout.size(),
            nLockTime);
        for (int i = 0; i < vin.size(); i++)
            str += "    " + vin[i].ToString() + "\n";
        for (int i = 0; i < vout.size(); i++)
            str += "    " + vout[i].ToString() + "\n";
        return str;
    }

    void print() const
    {
        printf("%s", ToString().c_str());
    }


    // 用于断开一笔交易输入所引用的上一笔交易输出与当前交易的链接
    // 在比特币节点运行过程中，可能需要对已经存在的交易进行修改或回滚操作，
    // 例如取消某笔交易的确认、恢复本地钱包的可用余额等。
    // DisconnectInputs函数可以帮助快速断开当前交易与已消费的上一笔交易输出之间的链接，
    // 并支持后续的处理和管理操作。
    bool DisconnectInputs(CTxDB& txdb);
    // 用于连接一笔交易输入与其所引用的上一笔交易输出
    bool ConnectInputs(CTxDB& txdb, map<uint256, CTxIndex>& mapTestPool, CDiskTxPos posThisTx, int nHeight, int64& nFees, bool fBlock, bool fMiner, int64 nMinFee=0);
    bool ClientConnectInputs();

    // 用于接受一笔新的比特币交易并将其添加到本地节点的交易池中
    bool AcceptTransaction(CTxDB& txdb, bool fCheckInputs=true, bool* pfMissingInputs=NULL);

    bool AcceptTransaction(bool fCheckInputs=true, bool* pfMissingInputs=NULL)
    {
        CTxDB txdb("r");
        return AcceptTransaction(txdb, fCheckInputs, pfMissingInputs);
    }

protected:
    // 将一笔新的比特币交易添加到本地节点的交易池中
    // 在比特币节点运行过程中，可能会不断接收新的交易，
    // 并需要对这些交易进行验证和处理后再加入本地交易池。
    // AddToMemoryPool函数可以帮助快速将新的交易添加到内存池中，
    // 并进行相应的验证和处理操作，以支持后续的交易确认、区块生成等功能。
    bool AddToMemoryPool();
public:
    bool RemoveFromMemoryPool();
};





//
// A transaction with a merkle branch linking it to the block chain
//
// 表示具有Merkle树结构的比特币交易
// 在比特币网络中，每个区块都包含多笔交易，并通过Merkle树的方式将这些交易组织起来。
// 通过使用CMerkleTx类，可以方便地处理比特币交易的Merkle树结构，并支持相关功能的实现和扩展。
class CMerkleTx : public CTransaction
{
public:
    // 表示当前交易所属的区块哈希值
    uint256 hashBlock;
    vector<uint256> vMerkleBranch;
    int nIndex;

    // memory only
    mutable bool fMerkleVerified;


    CMerkleTx()
    {
        Init();
    }

    CMerkleTx(const CTransaction& txIn) : CTransaction(txIn)
    {
        Init();
    }

    void Init()
    {
        hashBlock = 0;
        nIndex = -1;
        fMerkleVerified = false;
    }

    int64 GetCredit() const
    {
        // Must wait until coinbase is safely deep enough in the chain before valuing it
        if (IsCoinBase() && GetBlocksToMaturity() > 0)
            return 0;
        return CTransaction::GetCredit();
    }

    IMPLEMENT_SERIALIZE
    (
        nSerSize += SerReadWrite(s, *(CTransaction*)this, nType, nVersion, ser_action);
        nVersion = this->nVersion;
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    )


    // 设置一笔比特币交易在Merkle树中对应的哈希值路径（Merkle Branch）
    // 在比特币网络中，每个区块都包含多笔交易，并通过Merkle树的方式将这些交易组织起来。
    // 为当前区块中的每笔交易设置对应的Merkle Branch信息。
    // 该函数需要输入Merkle树中所有节点的哈希值
    // ，并根据这些哈希值计算出当前交易在Merkle树中的哈希值路径，以支持后续的验证和处理操作。
    int SetMerkleBranch(const CBlock* pblock=NULL);
    // ，用于获取当前交易在主链上的深度
    // 。在比特币核心代码中，GetDepthInMainChain函数会遍历区块链
    // ，计算当前交易在主链上的深度或高度。从当前区块开始，一直追溯到创世块
    // ，统计中间经过的区块数量即为当前交易在主链上的深度或高度。
    // 通过使用GetDepthInMainChain函数，可以方便地获取当前交易在区块链中的位置信息，
    // 并支持相关功能的实现和扩展。
    int GetDepthInMainChain() const;
    // 判断当前交易是否在主链上
    // 遍历区块链，查找当前交易所属的区块是否在主链上。
    // 从当前区块开始，一直追溯到创世块，逐个检查每个区块是否在主链上，并将结果返回。
    // 通过使用IsInMainChain函数，可以方便地判断当前交易是否已经被确认
    bool IsInMainChain() const { return GetDepthInMainChain() > 0; }
    // 计算某个UTXO在当前高度下需要多少个区块才能被花费（到达成熟度）。
    // 在比特币网络中，每个交易输出都需要等待一定数量的区块确认后才能被其他交易引用。
    // 该函数需要输入当前UTXO所在的区块高度以及当前区块链的最新高度，
    // 根据比特币协议规则计算出UTXO的成熟度，以支持后续的钱包管理和交易处理操作。
    int GetBlocksToMaturity() const;
    bool AcceptTransaction(CTxDB& txdb, bool fCheckInputs=true);
    bool AcceptTransaction() { CTxDB txdb("r"); return AcceptTransaction(txdb); }
};




//
// A transaction with a bunch of additional info that only the owner cares
// about.  It includes any unrecorded transactions needed to link it back
// to the block chain.
//
class CWalletTx : public CMerkleTx
{
public:
    vector<CMerkleTx> vtxPrev;
    map<string, string> mapValue;
    vector<pair<string, string> > vOrderForm;
    // 表示是否使用接收时间作为交易时间
    //由节点配置文件或命令行参数控制。如果该变量为true，
    // 则节点将使用接收时间作为交易时间；如果为false，则节点将使用交易本身的时间戳作为交易时间。
    unsigned int fTimeReceivedIsTxTime;
    // 表示交易在节点接收到的时间戳
    // 用于计算交易的确认时间、优先级等信息
    unsigned int nTimeReceived;  // time received by this node
    // 表示交易是否由本地节点发起
    // fFromMe变量通常在交易处理过程中被赋值，并用于区分交易来源和目标，
    // 以支持后续的处理和管理操作。如果该变量为true，则表示当前交易是由本地节点发起的
    char fFromMe;
    // 表示某个交易输出是否已经被花费
    // 在比特币网络中，每个交易输出都有一个对应的交易输入用于花费该输出。
    // fSpent变量通常在交易处理过程中被赋值，
    // 并用于支持钱包余额计算、双重支付检测等功能。
    // 如果该变量为true，则表示当前交易输出已经被花费
    char fSpent;
    //// probably need to sign the order info so know it came from payer

    // memory only
    mutable unsigned int nTimeDisplayed;


    CWalletTx()
    {
        Init();
    }

    CWalletTx(const CMerkleTx& txIn) : CMerkleTx(txIn)
    {
        Init();
    }

    CWalletTx(const CTransaction& txIn) : CMerkleTx(txIn)
    {
        Init();
    }

    void Init()
    {
        fTimeReceivedIsTxTime = false;
        nTimeReceived = 0;
        fFromMe = false;
        fSpent = false;
        nTimeDisplayed = 0;
    }

    IMPLEMENT_SERIALIZE
    (
        nSerSize += SerReadWrite(s, *(CMerkleTx*)this, nType, nVersion, ser_action);
        nVersion = this->nVersion;
        READWRITE(vtxPrev);
        READWRITE(mapValue);
        READWRITE(vOrderForm);
        READWRITE(fTimeReceivedIsTxTime);
        READWRITE(nTimeReceived);
        READWRITE(fFromMe);
        READWRITE(fSpent);
    )

    bool WriteToDisk()
    {
        return CWalletDB().WriteTx(GetHash(), *this);
    }


    // 用于获取一笔比特币交易的时间戳
    // 在比特币网络中，每笔交易都会有一个时间戳记录其创建时间。
    int64 GetTxTime() const;

    // 用于将一笔比特币交易所依赖的其他交易添加到本地节点的交易池中。
    // 在比特币网络中，每个交易可能会依赖和引用其他未确认的交易。
    void AddSupportingTransactions(CTxDB& txdb);

    // 用于接受一笔新的比特币交易并将其添加到本地节点的交易池中，
    // 并同时更新钱包相关信息。在比特币网络中，
    // 每个节点维护着自己的交易池，其中包含待确认的交易。
    bool AcceptWalletTransaction(CTxDB& txdb, bool fCheckInputs=true);
    bool AcceptWalletTransaction() { CTxDB txdb("r"); return AcceptWalletTransaction(txdb); }

    void RelayWalletTransaction(CTxDB& txdb);
    void RelayWalletTransaction() { CTxDB txdb("r"); RelayWalletTransaction(txdb); }
};




//
// A txdb record that contains the disk location of a transaction and the
// locations of transactions that spend its outputs.  vSpent is really only
// used as a flag, but having the location is very helpful for debugging.
//
// 用于维护一笔比特币交易在区块链中的位置信息
// CTxIndex通常被定义为CMerkleTx类的成员变量之一，用于记录当前交易在区块链中的位置、状态和其他相关信息
// 通过使用CTxIndex数据结构，可以方便地获取当前交易在区块链中的位置信息，并支持相关功能的实现和扩展。
class CTxIndex
{
public:
    // 用于记录一笔比特币交易在磁盘上的存储位置
    CDiskTxPos pos;
    vector<CDiskTxPos> vSpent;

    CTxIndex()
    {
        // 清空所有字段并将其设置为默认值
        SetNull();
    }

    CTxIndex(const CDiskTxPos& posIn, unsigned int nOutputs)
    {
        pos = posIn;
        vSpent.resize(nOutputs);
    }

    // 比特币核心代码中的一个宏，用于定义一个类的序列化和反序列化方法
    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(pos);
        READWRITE(vSpent);
    )

    void SetNull()
    {
        pos.SetNull();
        vSpent.clear();
    }

    bool IsNull()
    {
        return pos.IsNull();
    }

    friend bool operator==(const CTxIndex& a, const CTxIndex& b)
    {
        if (a.pos != b.pos || a.vSpent.size() != b.vSpent.size())
            return false;
        for (int i = 0; i < a.vSpent.size(); i++)
            if (a.vSpent[i] != b.vSpent[i])
                return false;
        return true;
    }

    friend bool operator!=(const CTxIndex& a, const CTxIndex& b)
    {
        return !(a == b);
    }
};





//
// Nodes collect new transactions into a block, hash them into a hash tree,
// and scan through nonce values to make the block's hash satisfy proof-of-work
// requirements.  When they solve the proof-of-work, they broadcast the block
// to everyone and the block is added to the block chain.  The first transaction
// in the block is a special one that creates a new coin owned by the creator
// of the block.
//
// Blocks are appended to blk0001.dat files on disk.  Their location on disk
// is indexed by CBlockIndex objects in memory.
//
// 用于表示一个区块
class CBlock
{
public:
    // header
    int nVersion;
    // 表示当前区块的前一区块的哈希值
    uint256 hashPrevBlock;
    // 表示当前区块所有交易的Merkle树根节点哈希值
    uint256 hashMerkleRoot;
    unsigned int nTime;
    // 用于表示当前区块难度目标的值
    unsigned int nBits;
    // 表示当前区块的Nonce值
    // 用于工作量证明（Proof-of-Work）。
    unsigned int nNonce;

    // network and disk
    // 表示当前区块包含的交易列表
    // 用于记录当前区块的所有交易
    vector<CTransaction> vtx;

    // memory only
    mutable vector<uint256> vMerkleTree;


    CBlock()
    {
        SetNull();
    }

    IMPLEMENT_SERIALIZE
    (
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);

        // ConnectBlock depends on vtx being last so it can calculate offset
        if (!(nType & (SER_GETHASH|SER_BLOCKHEADERONLY)))
            READWRITE(vtx);
        else if (fRead)
            const_cast<CBlock*>(this)->vtx.clear();
    )

    void SetNull()
    {
        nVersion = 1;
        hashPrevBlock = 0;
        hashMerkleRoot = 0;
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        vtx.clear();
        vMerkleTree.clear();
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash() const
    {
        return Hash(BEGIN(nVersion), END(nNonce));
    }


    // 在比特币网络中，交易列表通过Merkle树进行组织和计算
    // 如果交易列表为空，则返回空哈希值作为Merkle树根节点；
    // 如果交易列表仅包含一笔交易，则返回该交易的哈希值作为Merkle树根节点；
    // 如果交易列表包含多笔交易，则将交易列表按照顺序依次进行双重SHA-256哈希运算，
    // 并将结果两两配对，将配对结果再次进行哈希运算，直到得到最后的Merkle树根节点哈希值。
    uint256 BuildMerkleTree() const
    {
        vMerkleTree.clear();
        foreach(const CTransaction& tx, vtx)
            vMerkleTree.push_back(tx.GetHash());
        int j = 0;
        for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
        {
            for (int i = 0; i < nSize; i += 2)
            {
                int i2 = min(i+1, nSize-1);
                vMerkleTree.push_back(Hash(BEGIN(vMerkleTree[j+i]),  END(vMerkleTree[j+i]),
                                           BEGIN(vMerkleTree[j+i2]), END(vMerkleTree[j+i2])));
            }
            j += nSize;
        }
        return (vMerkleTree.empty() ? 0 : vMerkleTree.back());
    }

    // 获取某笔交易在Merkle树中的哈希值和证明路径
    // 根据交易哈希值找到该交易在交易列表中的索引；
    // 初始化证明路径为空列表；
    // 从底层叶子节点开始，依次沿着Merkle树向上遍历，记录每个与之配对的节点的哈希值，直到达到根节点；
    // 如果目标交易在左子树中，则将右子树节点的哈希值添加到证明路径中，并继续向上遍历；
    // 如果目标交易在右子树中，则将左子树节点的哈希值添加到证明路径中，并继续向上遍历；
    // 当到达根节点时，返回该节点的哈希值和证明路径。
    vector<uint256> GetMerkleBranch(int nIndex) const
    {
        if (vMerkleTree.empty())
            BuildMerkleTree();
        vector<uint256> vMerkleBranch;
        int j = 0;
        for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
        {
            int i = min(nIndex^1, nSize-1);
            vMerkleBranch.push_back(vMerkleTree[j+i]);
            nIndex >>= 1;
            j += nSize;
        }
        return vMerkleBranch;
    }

    // 验证某笔交易的证明路径是否有效
    // 根据交易哈希值找到该交易在交易列表中的索引；
    // 从底层叶子节点开始，依次沿着Merkle树向上遍历，并按照证明路径中的节点顺序取出对应的哈希值，
    // 直到达到根节点；
    // 通过对左右子树节点的哈希值进行双重SHA-256哈希运算，得到当前节点的哈希值；
    // 如果最终得到的哈希值与Merkle树根节点哈希值相等，则证明该交易的证明路径是有效的；
    // 否则，证明路径无效。
    static uint256 CheckMerkleBranch(uint256 hash, const vector<uint256>& vMerkleBranch, int nIndex)
    {
        if (nIndex == -1)
            return 0;
        foreach(const uint256& otherside, vMerkleBranch)
        {
            if (nIndex & 1)
                hash = Hash(BEGIN(otherside), END(otherside), BEGIN(hash), END(hash));
            else
                hash = Hash(BEGIN(hash), END(hash), BEGIN(otherside), END(otherside));
            nIndex >>= 1;
        }
        return hash;
    }


    // 用于将某个对象写入到磁盘上的操作
    // 将某个对象进行序列化后，写入到磁盘文件中的一个过程
    // 将区块链、交易池、UTXO集合等数据写入到磁盘文件中
    // 将钱包交易历史、地址列表等信息写入到磁盘文件中
    bool WriteToDisk(bool fWriteTransactions, unsigned int& nFileRet, unsigned int& nBlockPosRet)
    {
        // Open history file to append
        CAutoFile fileout = AppendBlockFile(nFileRet);
        if (!fileout)
            return error("CBlock::WriteToDisk() : AppendBlockFile failed");
        if (!fWriteTransactions)
            fileout.nType |= SER_BLOCKHEADERONLY;

        // Write index header
        unsigned int nSize = fileout.GetSerializeSize(*this);
        fileout << FLATDATA(pchMessageStart) << nSize;

        // Write block
        nBlockPosRet = ftell(fileout);
        if (nBlockPosRet == -1)
            return error("CBlock::WriteToDisk() : ftell failed");
        fileout << *this;

        return true;
    }

    // 从磁盘文件中读取二进制数据，并将其反序列化后得到某个对象的过程
    bool ReadFromDisk(unsigned int nFile, unsigned int nBlockPos, bool fReadTransactions)
    {
        SetNull();

        // Open history file to read
        CAutoFile filein = OpenBlockFile(nFile, nBlockPos, "rb");
        if (!filein)
            return error("CBlock::ReadFromDisk() : OpenBlockFile failed");
        if (!fReadTransactions)
            filein.nType |= SER_BLOCKHEADERONLY;

        // Read block
        filein >> *this;

        // Check the header
        if (CBigNum().SetCompact(nBits) > bnProofOfWorkLimit)
            return error("CBlock::ReadFromDisk() : nBits errors in block header");
        if (GetHash() > CBigNum().SetCompact(nBits).getuint256())
            return error("CBlock::ReadFromDisk() : GetHash() errors in block header");

        return true;
    }



    void print() const
    {
        printf("CBlock(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%d)\n",
            GetHash().ToString().substr(0,14).c_str(),
            nVersion,
            hashPrevBlock.ToString().substr(0,14).c_str(),
            hashMerkleRoot.ToString().substr(0,6).c_str(),
            nTime, nBits, nNonce,
            vtx.size());
        for (int i = 0; i < vtx.size(); i++)
        {
            printf("  ");
            vtx[i].print();
        }
        printf("  vMerkleTree: ");
        for (int i = 0; i < vMerkleTree.size(); i++)
            printf("%s ", vMerkleTree[i].ToString().substr(0,6).c_str());
        printf("\n");
    }


    // 计算挖矿所得的奖励金额
    int64 GetBlockValue(int64 nFees) const;
    bool DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex);
    bool ConnectBlock(CTxDB& txdb, CBlockIndex* pindex);
    bool ReadFromDisk(const CBlockIndex* blockindex, bool fReadTransactions);
    // 将新的区块添加到本地区块索引中。在比特币网络中
    // ，每当一个矿工成功地生成一个新的区块并广播到网络中时，
    // 其他节点需要将该区块添加到本地的区块链中，并建立索引以支持快速查询和访问。
    // 在将新区块添加到本地索引时，AddToBlockIndex函数通常会执行以下操作：
    // 将该区块的哈希值作为索引键值，将该区块的相关信息（例如，高度、前一区块的哈希值、时间戳等）存储到本地数据库中；
    // 更新最长区块链的相关信息，以支持同步和共识机制；
    // 将该区块所包含的所有交易添加到交易池中，以支持后续交易验证和处理。
    bool AddToBlockIndex(unsigned int nFile, unsigned int nBlockPos);
    // 验证新的区块是否符合比特币网络协议规定的各项规则要求
    // 验证区块头的工作量证明是否有效；
    // 验证区块中所有交易的有效性，并更新UTXO集合；
    // 验证区块中的所有交易的输入都指向已经存在的、未花费的输出；
    // 验证该区块的时间戳不早于前一个区块，并且区块高度连续；
    // 检查该区块的大小是否超出了最大限制，以及区块中交易数量是否超过了限制；
    // 检查区块中的Coinbase交易是否符合规则要求（例如，是否只包含一个输入和一个输出）；
    // 检查区块中的所有交易的签名和脚本是否符合规则要求；
    // 检查区块中的双花攻击是否合法。
    bool CheckBlock() const;
    // 用于接受、验证和处理新的区块
    // 在接受新区块时，AcceptBlock函数通常会执行以下操作：
    // 验证区块头的工作量证明是否有效；
    // 验证区块中所有交易的有效性，并更新UTXO集合；
    // 验证区块中的所有交易的输入都指向已经存在的、未花费的输出；
    // 验证该区块的时间戳不早于前一个区块，并且区块高度连续；
    // 更新区块链高度和状态，并将该区块存储到本地数据库中；
    // 广播该区块到网络中，以便其他节点更新状态。
    bool AcceptBlock();
};






//
// The block chain is a tree shaped structure starting with the
// genesis block at the root, with each block potentially having multiple
// candidates to be the next block.  pprev and pnext link a path through the
// main/longest chain.  A blockindex may have multiple pprev pointing back
// to it, but pnext will only point forward to the longest branch, or will
// be null if the block is not part of the longest chain.
//
// 表示区块链中的一个块的索引
class CBlockIndex
{
public:
    // 表示该区块的哈希值
    const uint256* phashBlock;
    // 指向前一个区块的CBlockIndex指针；
    CBlockIndex* pprev;
    CBlockIndex* pnext;
    // 表示该区块所在的磁盘文件编号
    unsigned int nFile;
    // 表示该区块在磁盘文件中的偏移位置
    unsigned int nBlockPos;
    // 表示该区块的高度
    int nHeight;

    // block header
    int nVersion;
    uint256 hashMerkleRoot;
    unsigned int nTime;
    unsigned int nBits;
    unsigned int nNonce;


    CBlockIndex()
    {
        phashBlock = NULL;
        pprev = NULL;
        pnext = NULL;
        nFile = 0;
        nBlockPos = 0;
        nHeight = 0;

        nVersion       = 0;
        hashMerkleRoot = 0;
        nTime          = 0;
        nBits          = 0;
        nNonce         = 0;
    }

    CBlockIndex(unsigned int nFileIn, unsigned int nBlockPosIn, CBlock& block)
    {
        phashBlock = NULL;
        pprev = NULL;
        pnext = NULL;
        nFile = nFileIn;
        nBlockPos = nBlockPosIn;
        nHeight = 0;

        nVersion       = block.nVersion;
        hashMerkleRoot = block.hashMerkleRoot;
        nTime          = block.nTime;
        nBits          = block.nBits;
        nNonce         = block.nNonce;
    }

    uint256 GetBlockHash() const
    {
        return *phashBlock;
    }

    bool IsInMainChain() const
    {
        return (pnext || this == pindexBest);
    }

    // 从磁盘上删除某个块以进行数据清理或回收空间
    // 在删除一个块时，EraseBlockFromDisk函数通常会执行以下操作：
    // 从本地区块索引中删除该块的相关信息；
    // 删除该块对应的磁盘文件中的数据；
    // 删除该块所包含的所有交易的相关数据。
    bool EraseBlockFromDisk()
    {
        // Open history file
        CAutoFile fileout = OpenBlockFile(nFile, nBlockPos, "rb+");
        if (!fileout)
            return false;

        // Overwrite with empty null block
        CBlock block;
        block.SetNull();
        fileout << block;

        return true;
    }

    enum { nMedianTimeSpan=11 };

    // 计算一组区块的中位时间戳
    // 在比特币网络中，由于每个区块的时间戳可能存在不准确或篡改的风险，
    // 因此不仅需要计算区块的中位时间戳，还需要计算最近11个区块的时间戳跨度的中位数，
    // 以确保网络的稳定性和安全性。
    // 如果某个区块的时间戳与其前一个区块的时间戳之间的跨度超过了nMedianTimeSpan的两倍
    // ，则该区块将被认为是无效的，并被其他节点拒绝。
    int64 GetMedianTimePast() const
    {
        unsigned int pmedian[nMedianTimeSpan];
        unsigned int* pbegin = &pmedian[nMedianTimeSpan];
        unsigned int* pend = &pmedian[nMedianTimeSpan];

        const CBlockIndex* pindex = this;
        for (int i = 0; i < nMedianTimeSpan && pindex; i++, pindex = pindex->pprev)
            *(--pbegin) = pindex->nTime;

        sort(pbegin, pend);
        return pbegin[(pend - pbegin)/2];
    }

    // 计算一组区块的中位时间
    // 与"GetMedianTimePast"函数不同，"GetMedianTime"函数同时考虑了最近11个区块和最近20,160个区块
    // （约两周）的时间戳，以得到更加稳定和可靠的结果。
    // 在计算中位时间戳时，GetMedianTime函数通常会执行以下操作：
    // 获取最近11个区块和最近20,160个区块的时间戳；
    // 将这些时间戳按从早到晚的顺序排序；
    // 返回排序后的第(n+1)/2个时间戳作为中位时间戳，其中n为排序后的时间戳数量。
    int64 GetMedianTime() const
    {
        const CBlockIndex* pindex = this;
        for (int i = 0; i < nMedianTimeSpan/2; i++)
        {
            if (!pindex->pnext)
                return nTime;
            pindex = pindex->pnext;
        }
        return pindex->GetMedianTimePast();
    }



    string ToString() const
    {
        return strprintf("CBlockIndex(nprev=%08x, pnext=%08x, nFile=%d, nBlockPos=%-6d nHeight=%d, merkle=%s, hashBlock=%s)",
            pprev, pnext, nFile, nBlockPos, nHeight,
            hashMerkleRoot.ToString().substr(0,6).c_str(),
            GetBlockHash().ToString().substr(0,14).c_str());
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};



//
// Used to marshal pointers into hashes for db storage.
//
// 表示磁盘上的块索引。由于比特币区块链很长，不能将所有块都加载到内存中，
// 因此需要一种方法来管理磁盘上的块索引。
class CDiskBlockIndex : public CBlockIndex
{
public:
    uint256 hashPrev;
    uint256 hashNext;

    CDiskBlockIndex()
    {
        hashPrev = 0;
        hashNext = 0;
    }

    explicit CDiskBlockIndex(CBlockIndex* pindex) : CBlockIndex(*pindex)
    {
        hashPrev = (pprev ? pprev->GetBlockHash() : 0);
        hashNext = (pnext ? pnext->GetBlockHash() : 0);
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);

        READWRITE(hashNext);
        READWRITE(nFile);
        READWRITE(nBlockPos);
        READWRITE(nHeight);

        // block header
        READWRITE(this->nVersion);
        READWRITE(hashPrev);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);
    )

    uint256 GetBlockHash() const
    {
        CBlock block;
        block.nVersion        = nVersion;
        block.hashPrevBlock   = hashPrev;
        block.hashMerkleRoot  = hashMerkleRoot;
        block.nTime           = nTime;
        block.nBits           = nBits;
        block.nNonce          = nNonce;
        return block.GetHash();
    }


    string ToString() const
    {
        string str = "CDiskBlockIndex(";
        str += CBlockIndex::ToString();
        str += strprintf("\n                hashBlock=%s, hashPrev=%s, hashNext=%s)",
            GetBlockHash().ToString().c_str(),
            hashPrev.ToString().substr(0,14).c_str(),
            hashNext.ToString().substr(0,14).c_str());
        return str;
    }

    void print() const
    {
        printf("%s\n", ToString().c_str());
    }
};








//
// Describes a place in the block chain to another node such that if the
// other node doesn't have the same branch, it can find a recent common trunk.
// The further back it is, the further before the fork it may be.
//
// 表示块的定位器。在比特币网络中，每个节点需要了解其他节点拥有哪些块以便进行同步和共识。
class CBlockLocator
{
protected:
    // 表示该定位器所代表的一系列块的哈希值
    vector<uint256> vHave;
    // nHashStop：表示该定位器停止搜索的块的哈希值。
public:

    CBlockLocator()
    {
    }

    explicit CBlockLocator(const CBlockIndex* pindex)
    {
        Set(pindex);
    }

    explicit CBlockLocator(uint256 hashBlock)
    {
        map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end())
            Set((*mi).second);
    }

    IMPLEMENT_SERIALIZE
    (
        if (!(nType & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vHave);
    )

    void Set(const CBlockIndex* pindex)
    {
        vHave.clear();
        int nStep = 1;
        while (pindex)
        {
            vHave.push_back(pindex->GetBlockHash());

            // Exponentially larger steps back
            for (int i = 0; pindex && i < nStep; i++)
                pindex = pindex->pprev;
            if (vHave.size() > 10)
                nStep *= 2;
        }
        vHave.push_back(hashGenesisBlock);
    }

    CBlockIndex* GetBlockIndex()
    {
        // Find the first block the caller has in the main chain
        foreach(const uint256& hash, vHave)
        {
            map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
            if (mi != mapBlockIndex.end())
            {
                CBlockIndex* pindex = (*mi).second;
                if (pindex->IsInMainChain())
                    return pindex;
            }
        }
        return pindexGenesisBlock;
    }

    // 获取比特币区块链中指定高度的区块哈希。
    // 这个命令通常被用来检索先前发生的交易，或者确认一个区块是否已经被添加到区块链中。
    uint256 GetBlockHash()
    {
        // Find the first block the caller has in the main chain
        foreach(const uint256& hash, vHave)
        {
            map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hash);
            if (mi != mapBlockIndex.end())
            {
                CBlockIndex* pindex = (*mi).second;
                if (pindex->IsInMainChain())
                    return hash;
            }
        }
        return hashGenesisBlock;
    }

    int GetHeight()
    {
        CBlockIndex* pindex = GetBlockIndex();
        if (!pindex)
            return 0;
        return pindex->nHeight;
    }
};












extern map<uint256, CTransaction> mapTransactions;
extern map<uint256, CWalletTx> mapWallet;
// 指示钱包状态已发生更改或更新
extern vector<pair<uint256, bool> > vWalletUpdated;
// 用于存储钱包地址和相关信息之间的映射关系
// 键是钱包地址，值是钱包地址对应的信息，如余额、交易历史等
extern CCriticalSection cs_mapWallet;
// 存储私钥和相关信息之间的映射关系
// 键是私钥，值是私钥对应的信息，如地址、余额、交易历史等
extern map<vector<unsigned char>, CPrivKey> mapKeys;
// 存储公钥与相关信息之间的映射关系
// 键是公钥，值是公钥对应的信息，如地址、余额、交易历史等
extern map<uint160, vector<unsigned char> > mapPubKeys;
// 存储私钥和相关信息之间的映射关系
extern CCriticalSection cs_mapKeys;
// 表示与用户相关的私钥或公钥
extern CKey keyUser;
