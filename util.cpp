// Copyright (c) 2009 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"



bool fDebug = false;




// HANDLE是Windows操作系统中的一个数据类型，
// 它是一个指向对象的引用。
// 在Windows API编程中，
// 很多函数都会返回一个HANDLE类型的值，
// 用于表示某种系统资源的句柄，
// 例如文件、进程、线程等。HANDLE类型本身并不包含任何数据，
// 只是一个可以唯一标识系统资源的引用。
// 开发人员可以使用一系列的系统函数来操作这些系统资源，
// 例如读写文件、创建进程、等待线程结束等。需要注意的是，
// 在使用完HANDLE类型对应的系统资源后，必须调用相应的系统函数来释放资源，
// 否则可能会产生内存泄漏或其他问题。
// Init openssl library multithreading support
static HANDLE* lock_cs;

void win32_locking_callback(int mode, int type, const char* file, int line)
{
    if (mode & CRYPTO_LOCK)
        WaitForSingleObject(lock_cs[type], INFINITE);
    else
        ReleaseMutex(lock_cs[type]);
}

// Init
// class CInit是比特币源代码中的一个类，
// 用于执行比特币核心组件的初始化操作。
// 在比特币程序启动时，CInit类会被实例化，
// 并且会调用一系列的初始化函数来设置比特币的各种参数和配置。
// 这些初始化函数包括加载配置文件、初始化日志、初始化网络等。
// CInit类还提供了一些工具函数，
// 例如获取比特币程序所在的目录、获取PID文件的路径等。
// CInit类是比特币程序启动的重要组成部分，它确保了比特币程序在启动时的正确性和健壮性。
class CInit
{
public:
    CInit()
    {
        // Init openssl library multithreading support
        lock_cs = (HANDLE*)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(HANDLE));
        for (int i = 0; i < CRYPTO_num_locks(); i++)
            lock_cs[i] = CreateMutex(NULL,FALSE,NULL);
        CRYPTO_set_locking_callback(win32_locking_callback);

        // Seed random number generator with screen scrape and other hardware sources
        RAND_screen();

        // Seed random number generator with perfmon data
        RandAddSeed(true);
    }
    ~CInit()
    {
        // Shutdown openssl library multithreading support
        CRYPTO_set_locking_callback(NULL);
        for (int i =0 ; i < CRYPTO_num_locks(); i++)
            CloseHandle(lock_cs[i]);
        OPENSSL_free(lock_cs);
    }
}
instance_of_cinit;



// RandAddSeed是比特币源代码中的一个函数，
// 用于为比特币随机数生成器添加种子。
// 在比特币中，随机数生成器被广泛用于密码学和网络协议等方面，
// 因此其安全性和随机性至关重要。
// RandAddSeed函数可以通过传入一些随机的种子数据，
// 来增加随机数生成器的随机性。
// 这些种子数据可以来自于操作系统的随机数生成器、硬件设备的噪声、网络流量等。
// 在比特币代码中，RandAddSeed函数通常在程序启动时被调用，
// 以提高随机数生成器的质量和安全性。
void RandAddSeed(bool fPerfmon)
{
    // Seed with CPU performance counter
    // LARGE_INTEGER是Windows操作系统中的一个数据类型，
    // 用于表示64位整数。它实际上是一个联合体，
    // 包含了两个成员变量：LowPart和HighPart。
    // 这两个成员变量分别表示64位整数的低32位和高32位。
    // 通过访问这两个成员变量，可以获取或设置LARGE_INTEGER类型的值。
    // LARGE_INTEGER通常被用于Windows API中一些需要处理大整数的函数中，
    // 例如计时器函数、文件大小函数等。
    LARGE_INTEGER PerformanceCount;
    // QueryPerformanceCounter是Windows操作系统中的一个函数，
    // 用于获取高精度计时器的值。
    // 在Windows系统中，由于硬件计时器和操作系统时间片等因素的影响，
    // 使用普通的系统时钟来进行计时可能会出现不准确的情况。
    // 为了解决这个问题，Windows引入了高精度计时器（High-Resolution Timer），
    // 提供了更加准确的计时方式。
    // QueryPerformanceCounter函数可以获取当前系统的高精度计时器的值，
    // 单位为计时器频率下的计数值。与普通的系统时钟相比，
    // 高精度计时器能够提供更高的精度和稳定性，因此被广泛应用于计时、性能测试等方面。
    QueryPerformanceCounter(&PerformanceCount);
    RAND_add(&PerformanceCount, sizeof(PerformanceCount), 1.5);
    memset(&PerformanceCount, 0, sizeof(PerformanceCount));

    static int64 nLastPerfmon;
    if (fPerfmon || GetTime() > nLastPerfmon + 5 * 60)
    {
        nLastPerfmon = GetTime();

        // Seed with the entire set of perfmon data
        unsigned char pdata[250000];
        memset(pdata, 0, sizeof(pdata));
        unsigned long nSize = sizeof(pdata);
        long ret = RegQueryValueEx(HKEY_PERFORMANCE_DATA, "Global", NULL, NULL, pdata, &nSize);
        RegCloseKey(HKEY_PERFORMANCE_DATA);
        if (ret == ERROR_SUCCESS)
        {
            uint256 hash;
            SHA256(pdata, nSize, (unsigned char*)&hash);
            RAND_add(&hash, sizeof(hash), min(nSize/500.0, (double)sizeof(hash)));
            hash = 0;
            memset(pdata, 0, nSize);

            time_t nTime;
            time(&nTime);
            struct tm* ptmTime = gmtime(&nTime);
            char pszTime[200];
            strftime(pszTime, sizeof(pszTime), "%x %H:%M:%S", ptmTime);
            printf("%s  RandAddSeed() got %d bytes of performance data\n", pszTime, nSize);
        }
    }
}










// Safer snprintf
//  - prints up to limit-1 characters
//  - output string is always null terminated even if limit reached
//  - return value is the number of characters actually printed
int my_snprintf(char* buffer, size_t limit, const char* format, ...)
{
    if (limit == 0)
        return 0;
    va_list arg_ptr;
    va_start(arg_ptr, format);
    int ret = _vsnprintf(buffer, limit, format, arg_ptr);
    va_end(arg_ptr);
    if (ret < 0 || ret >= limit)
    {
        ret = limit - 1;
        buffer[limit-1] = 0;
    }
    return ret;
}


string strprintf(const char* format, ...)
{
    char buffer[50000];
    char* p = buffer;
    int limit = sizeof(buffer);
    int ret;
    loop
    {
        va_list arg_ptr;
        va_start(arg_ptr, format);
        ret = _vsnprintf(p, limit, format, arg_ptr);
        va_end(arg_ptr);
        if (ret >= 0 && ret < limit)
            break;
        if (p != buffer)
            delete p;
        limit *= 2;
        p = new char[limit];
        if (p == NULL)
            throw std::bad_alloc();
    }
#ifdef _MSC_VER
    // msvc optimisation
    if (p == buffer)
        return string(p, p+ret);
#endif
    string str(p, p+ret);
    if (p != buffer)
        delete p;
    return str;
}


bool error(const char* format, ...)
{
    char buffer[50000];
    int limit = sizeof(buffer);
    va_list arg_ptr;
    va_start(arg_ptr, format);
    int ret = _vsnprintf(buffer, limit, format, arg_ptr);
    va_end(arg_ptr);
    if (ret < 0 || ret >= limit)
    {
        ret = limit - 1;
        buffer[limit-1] = 0;
    }
    printf("ERROR: %s\n", buffer);
    return false;
}


void PrintException(std::exception* pex, const char* pszThread)
{
    char pszModule[260];
    pszModule[0] = '\0';
    GetModuleFileName(NULL, pszModule, sizeof(pszModule));
    _strlwr(pszModule);
    char pszMessage[1000];
    if (pex)
        snprintf(pszMessage, sizeof(pszMessage),
            "EXCEPTION: %s       \n%s       \n%s in %s       \n", typeid(*pex).name(), pex->what(), pszModule, pszThread);
    else
        snprintf(pszMessage, sizeof(pszMessage),
            "UNKNOWN EXCEPTION       \n%s in %s       \n", pszModule, pszThread);
    printf("\n\n************************\n%s", pszMessage);
    if (wxTheApp)
        wxMessageBox(pszMessage, "Error", wxOK | wxICON_ERROR);
    throw;
    //DebugBreak();
}


void ParseString(const string& str, char c, vector<string>& v)
{
    unsigned int i1 = 0;
    unsigned int i2;
    do
    {
        i2 = str.find(c, i1);
        v.push_back(str.substr(i1, i2-i1));
        i1 = i2+1;
    }
    while (i2 != str.npos);
}


string FormatMoney(int64 n, bool fPlus)
{
    n /= CENT;
    string str = strprintf("%I64d.%02I64d", (n > 0 ? n : -n)/100, (n > 0 ? n : -n)%100);
    for (int i = 6; i < str.size(); i += 4)
        if (isdigit(str[str.size() - i - 1]))
            str.insert(str.size() - i, 1, ',');
    if (n < 0)
        str.insert((unsigned int)0, 1, '-');
    else if (fPlus && n > 0)
        str.insert((unsigned int)0, 1, '+');
    return str;
}

// ParseMoney是比特币源代码中的一个函数
// ，用于将表示货币数量的字符串解析为整数值
// 。在比特币系统中，货币数量通常使用一种叫做“Satoshi”的单位来表示，
// 1 Satoshi等于0.00000001 BTC。
// ParseMoney函数会将输入的货币数量字符串按照指定的位数进行解析
// ，并返回对应的整数值。例如，如果输入的字符串为“1.23 BTC”，
// 且指定了以Satoshi为单位进行解析，
// 则函数将返回123000000（即1.23 BTC对应的Satoshis数量）。
// 在比特币交易和账户管理等方面，ParseMoney函数被广泛使用。
bool ParseMoney(const char* pszIn, int64& nRet)
{
    string strWhole;
    int64 nCents = 0;
    const char* p = pszIn;
    while (isspace(*p))
        p++;
    for (; *p; p++)
    {
        if (*p == ',' && p > pszIn && isdigit(p[-1]) && isdigit(p[1]) && isdigit(p[2]) && isdigit(p[3]) && !isdigit(p[4]))
            continue;
        if (*p == '.')
        {
            p++;
            if (isdigit(*p))
            {
                nCents = 10 * (*p++ - '0');
                if (isdigit(*p))
                    nCents += (*p++ - '0');
            }
            break;
        }
        if (isspace(*p))
            break;
        if (!isdigit(*p))
            return false;
        strWhole.insert(strWhole.end(), *p);
    }
    for (; *p; p++)
        if (!isspace(*p))
            return false;
    if (strWhole.size() > 14)
        return false;
    if (nCents < 0 || nCents > 99)
        return false;
    int64 nWhole = atoi64(strWhole);
    int64 nPreValue = nWhole * 100 + nCents;
    int64 nValue = nPreValue * CENT;
    if (nValue / CENT != nPreValue)
        return false;
    if (nValue / COIN != nWhole)
        return false;
    nRet = nValue;
    return true;
}









// FileExists是比特币源代码中的一个函数，
// 用于判断指定路径下的文件是否存在。
// 该函数接受一个字符串参数，表示文件的完整路径和文件名，
// 如果该文件存在，则返回true，否则返回false。
// 在比特币程序中，
// FileExists函数被广泛用于检查配置文件、PID文件、日志文件等系统文件是否存在。
// 如果这些文件不存在，则可能会导致比特币程序无法正常启动或运行。
// 因此，在比特币程序的启动过程中，使用FileExists函数来检查这些必要的文件是否存在非常重要。
bool FileExists(const char* psz)
{
#ifdef WIN32
    return GetFileAttributes(psz) != -1;
#else
    return access(psz, 0) != -1;
#endif
}

int GetFilesize(FILE* file)
{
    int nSavePos = ftell(file);
    int nFilesize = -1;
    if (fseek(file, 0, SEEK_END) == 0)
        nFilesize = ftell(file);
    fseek(file, nSavePos, SEEK_SET);
    return nFilesize;
}








uint64 GetRand(uint64 nMax)
{
    if (nMax == 0)
        return 0;

    // The range of the random source must be a multiple of the modulus
    // to give every possible output value an equal possibility
    uint64 nRange = (_UI64_MAX / nMax) * nMax;
    uint64 nRand = 0;
    do
        RAND_bytes((unsigned char*)&nRand, sizeof(nRand));
    while (nRand >= nRange);
    return (nRand % nMax);
}










//
// "Never go to sea with two chronometers; take one or three."
// Our three chronometers are:
//  - System clock
//  - Median of other server's clocks
//  - NTP servers
//
// note: NTP isn't implemented yet, so until then we just use the median
//  of other nodes clocks to correct ours.
//

int64 GetTime()
{
    return time(NULL);
}

static int64 nTimeOffset = 0;

int64 GetAdjustedTime()
{
    return GetTime() + nTimeOffset;
}

// AddTimeData是比特币源代码中的一个函数，
// 用于向时间同步服务添加时间数据。
// 在比特币网络中，各个节点需要保持时间的同步，
// 以确保交易和区块的时间戳是准确的。为了实现时间同步，
// 比特币引入了一种叫做NTP（Network Time Protocol）的协议，
// 允许节点与时间服务器进行时间同步。
// AddTimeData函数会从其他节点或时间服务器获取到当前时间的数据，
// 并将这些数据添加到本地的时间同步服务中。
// 比特币程序会定期使用时间同步服务来更新本地的系统时间，
// 以确保其与网络中的其他节点保持同步。
void AddTimeData(unsigned int ip, int64 nTime)
{
    int64 nOffsetSample = nTime - GetTime();

    // Ignore duplicates
    static set<unsigned int> setKnown;
    if (!setKnown.insert(ip).second)
        return;

    // Add data
    static vector<int64> vTimeOffsets;
    if (vTimeOffsets.empty())
        vTimeOffsets.push_back(0);
    vTimeOffsets.push_back(nOffsetSample);
    printf("Added time data, samples %d, ip %08x, offset %+I64d (%+I64d minutes)\n", vTimeOffsets.size(), ip, vTimeOffsets.back(), vTimeOffsets.back()/60);
    if (vTimeOffsets.size() >= 5 && vTimeOffsets.size() % 2 == 1)
    {
        sort(vTimeOffsets.begin(), vTimeOffsets.end());
        int64 nMedian = vTimeOffsets[vTimeOffsets.size()/2];
        nTimeOffset = nMedian;
        if ((nMedian > 0 ? nMedian : -nMedian) > 5 * 60)
        {
            // Only let other nodes change our clock so far before we
            // go to the NTP servers
            /// todo: Get time from NTP servers, then set a flag
            ///    to make sure it doesn't get changed again
        }
        foreach(int64 n, vTimeOffsets)
            printf("%+I64d  ", n);
        printf("|  nTimeOffset = %+I64d  (%+I64d minutes)\n", nTimeOffset, nTimeOffset/60);
    }
}
