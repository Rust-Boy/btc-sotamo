// Copyright (c) 2009 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"

// 用于将IRC聊天室中公告的其他节点的IP地址和端口号映射到比特币网络中。
// 该函数会解析IRC消息，提取有效的IP地址和端口号，并将其添加到一个内存映射表中，
// 以便比特币节点可以使用这些信息作为潜在的对等节点来建立连接并加入比特币网络。
// 这个函数主要用于增加比特币网络的可用性和去中心化程度。
map<vector<unsigned char>, CAddress> mapIRCAddresses;
// CCriticalSection是比特币核心代码中使用的一个类，
// 它提供了一种同步机制，以确保在多个线程同时访问共享资源时不会发生数据竞争或其他并发问题
// 。该类基于操作系统提供的原语（如互斥锁或信号量）实现，可以通过加锁和解锁来控制对关键区域的访问。
// 在比特币中，CCriticalSection常用于保护对内存池、块链、网络连接等数据结构的访问。
// cs_mapIRCAddresses是一个CCriticalSection类型的对象，
// 用于保护对mapIRCAddresses内存映射表的访问。
// 由于mapIRCAddresses是在多个线程之间共享的数据结构，
// 因此需要使用同步机制来避免并发问题。
// 通过在访问mapIRCAddresses之前加锁并在访问完成后解锁，
// 可以确保只有一个线程可以修改该表，从而保证其正确性和一致性。
CCriticalSection cs_mapIRCAddresses;




#pragma pack(push, 1)
// ircaddr是比特币协议中使用的一种地址类型，
// 用于表示通过IRC聊天室公告的其他节点的网络地址。
// 该地址类型由IP地址和端口号组成，并以字符串形式表示，例如：12.34.56.78:8333。
// 在比特币网络中，这些地址通常用于初始连接，即比特币节点启动时获取其他可用节点的方式之一。
struct ircaddr
{
    int ip;
    short port;
};
#pragma pack(pop)

// EncodeAddress是比特币核心代码中的一个函数，
// 用于将比特币地址（Bitcoin address）从内部表示形式转换为人类可读的字符串形式。
// 比特币地址由一串字母和数字组成，用于标识比特币账户。
// EncodeAddress函数接受一个公钥哈希值（即比特币地址的内部表示形式），
// 并返回其对应的字符串格式的比特币地址。该函数还支持指定比特币地址的网络类型（主网、测试网等）。
string EncodeAddress(const CAddress& addr)
{
    struct ircaddr tmp;
    tmp.ip    = addr.ip;
    tmp.port  = addr.port;

    vector<unsigned char> vch(UBEGIN(tmp), UEND(tmp));
    return string("u") + EncodeBase58Check(vch);
}

// DecodeAddress是比特币核心代码中的一个函数，用于将人类可读的比特币地址字符串转换为内部表示形式。
// 比特币地址由一串字母和数字组成，
// 用于标识比特币账户。DecodeAddress函数接受一个比特币地址字符串，
// 并返回其对应的公钥哈希值，即比特币地址的内部表示形式。
// 该函数还支持解析比特币地址的网络类型（主网、测试网等），并可以检查地址的有效性。
bool DecodeAddress(string str, CAddress& addr)
{
    vector<unsigned char> vch;
    if (!DecodeBase58Check(str.substr(1), vch))
        return false;

    struct ircaddr tmp;
    if (vch.size() != sizeof(tmp))
        return false;
    memcpy(&tmp, &vch[0], sizeof(tmp));

    addr  = CAddress(tmp.ip, tmp.port);
    return true;
}






static bool Send(SOCKET hSocket, const char* pszSend)
{
    if (strstr(pszSend, "PONG") != pszSend)
        printf("SENDING: %s\n", pszSend);
    const char* psz = pszSend;
    const char* pszEnd = psz + strlen(psz);
    while (psz < pszEnd)
    {
        int ret = send(hSocket, psz, pszEnd - psz, 0);
        if (ret < 0)
            return false;
        psz += ret;
    }
    return true;
}

// RecvLine是比特币核心代码中的一个函数，
// 用于从网络连接中读取一行数据（以换行符结尾）并返回该行数据的字符串表示。
// 在比特币网络中，节点之间通过网络连接进行通信
// ，而RecvLine则提供了一种方便的方式来读取这些数据流。
// 该函数会阻塞当前线程直到收到完整的一行数据或发生错误。
// 如果成功接收到数据，则会将其转换为字符串格式并返回；否则，函数将返回空字符串或抛出异常。
bool RecvLine(SOCKET hSocket, string& strLine)
{
    strLine = "";
    loop
    {
        char c;
        int nBytes = recv(hSocket, &c, 1, 0);
        if (nBytes > 0)
        {
            if (c == '\n')
                continue;
            if (c == '\r')
                return true;
            strLine += c;
        }
        else if (nBytes <= 0)
        {
            if (!strLine.empty())
                return true;
            // socket closed
            printf("IRC socket closed\n");
            return false;
        }
        else
        {
            // socket error
            int nErr = WSAGetLastError();
            if (nErr != WSAEMSGSIZE && nErr != WSAEINTR && nErr != WSAEINPROGRESS)
            {
                printf("IRC recv failed: %d\n", nErr);
                return false;
            }
        }
    }
}

// RecvLineIRC是比特币核心代码中的一个函数，
// 用于从IRC（Internet Relay Chat）聊天室中读取一行数据（以换行符结尾）并返回该行数据的字符串表示。
// 在比特币网络中，节点之间通过IRC聊天室进行通信，
// 而RecvLineIRC则提供了一种方便的方式来读取这些数据流。
// 该函数会阻塞当前线程直到收到完整的一行数据或发生错误。
// 如果成功接收到数据，则会将其转换为字符串格式并返回；否则，函数将返回空字符串或抛出异常。
bool RecvLineIRC(SOCKET hSocket, string& strLine)
{
    loop
    {
        bool fRet = RecvLine(hSocket, strLine);
        if (fRet)
        {
            if (fShutdown)
                return false;
            vector<string> vWords;
            ParseString(strLine, ' ', vWords);
            if (vWords[0] == "PING")
            {
                strLine[1] = 'O';
                strLine += '\r';
                Send(hSocket, strLine.c_str());
                continue;
            }
        }
        return fRet;
    }
}

// RecvUntil是比特币核心代码中的一个函数，
// 用于从网络连接中读取数据直到指定的结束标记出现，
// 并返回所有读取的数据。在比特币网络中，节点之间通过网络连接进行通信
// ，而RecvUntil则提供了一种方便的方式来读取这些数据流。
// 该函数会阻塞当前线程直到收到结束标记或发生错误。
// 如果成功接收到数据，则会将其转换为字符串格式并返回；
// 否则，函数将返回空字符串或抛出异常。
bool RecvUntil(SOCKET hSocket, const char* psz1, const char* psz2=NULL, const char* psz3=NULL)
{
    loop
    {
        string strLine;
        if (!RecvLineIRC(hSocket, strLine))
            return false;
        printf("IRC %s\n", strLine.c_str());
        if (psz1 && strLine.find(psz1) != -1)
            return true;
        if (psz2 && strLine.find(psz2) != -1)
            return true;
        if (psz3 && strLine.find(psz3) != -1)
            return true;
    }
}

// Wait(int nSeconds)是比特币核心代码中的一个函数，
// 用于让当前线程休眠指定的秒数。该函数会阻塞当前线程，
// 直到指定的时间过去或发生中断事件。函数返回一个布尔值
// ，表示休眠是否成功完成。如果在指定的时间内没有发生中断事件，
// 则返回true；否则，返回false。通常情况下，
// Wait函数用于等待一段时间后再执行某些操作，例如等待新块的到来或等待网络连接建立。
bool Wait(int nSeconds)
{
    if (fShutdown)
        return false;
    printf("Waiting %d seconds to reconnect to IRC\n", nSeconds);
    for (int i = 0; i < nSeconds; i++)
    {
        if (fShutdown)
            return false;
        Sleep(1000);
    }
    return true;
}



// ThreadIRCSeed是比特币核心代码中的一个函数，
// 用于在后台启动一个线程来连接IRC（Internet Relay Chat）聊天室，
// 并获取其他比特币节点的网络地址。
// 在比特币网络中，节点之间通过IRC聊天室进行通信，
// 而ThreadIRCSeed则提供了一种方便的方式来自动化这个过程。
// 该函数会在后台启动一个线程，并使用RecvLineIRC等其他函数来与IRC聊天室进行交互，
// 以获取其他比特币节点的IP地址和端口号信息。这些信息将被添加到内存映射表中，
// 以便比特币节点可以使用它们作为潜在的对等节点来建立连接并加入比特币网络。
void ThreadIRCSeed(void* parg)
{
    // SetThreadPriority是一个操作系统级别的函数，
    // 它可以用于设置线程的优先级。
    // 在比特币核心代码中，该函数被用于控制不同线程的执行顺序和响应性能。
    // 通过调整线程的优先级，可以让某些线程获得更多或更少的CPU时间片，
    // 并根据需要分配系统资源。该函数接受一个整数参数，表示要设置的线程优先级。
    // 通常情况下，较高的数字表示更高的优先级。在使用SetThreadPriority时需要小心，
    // 因为错误的优先级设置可能会导致应用程序出现异常行为或崩溃。
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    int nErrorWait = 10;
    int nRetryWait = 10;

    while (!fShutdown)
    {
        CAddress addrConnect("216.155.130.130:6667");
        struct hostent* phostent = gethostbyname("chat.freenode.net");
        if (phostent && phostent->h_addr_list && phostent->h_addr_list[0])
            addrConnect = CAddress(*(u_long*)phostent->h_addr_list[0], htons(6667));

        SOCKET hSocket;
        if (!ConnectSocket(addrConnect, hSocket))
        {
            printf("IRC connect failed\n");
            nErrorWait = nErrorWait * 11 / 10;
            if (Wait(nErrorWait += 60))
                continue;
            else
                return;
        }

        if (!RecvUntil(hSocket, "Found your hostname", "using your IP address instead", "Couldn't look up your hostname"))
        {
            closesocket(hSocket);
            nErrorWait = nErrorWait * 11 / 10;
            if (Wait(nErrorWait += 60))
                continue;
            else
                return;
        }

        string strMyName = EncodeAddress(addrLocalHost);

        if (!addrLocalHost.IsRoutable())
            strMyName = strprintf("x%u", GetRand(1000000000));


        Send(hSocket, strprintf("NICK %s\r", strMyName.c_str()).c_str());
        Send(hSocket, strprintf("USER %s 8 * : %s\r", strMyName.c_str(), strMyName.c_str()).c_str());

        if (!RecvUntil(hSocket, " 004 "))
        {
            closesocket(hSocket);
            nErrorWait = nErrorWait * 11 / 10;
            if (Wait(nErrorWait += 60))
                continue;
            else
                return;
        }
        Sleep(500);

        Send(hSocket, "JOIN #bitcoin\r");
        Send(hSocket, "WHO #bitcoin\r");

        int64 nStart = GetTime();
        string strLine;
        while (!fShutdown && RecvLineIRC(hSocket, strLine))
        {
            if (strLine.empty() || strLine.size() > 900 || strLine[0] != ':')
                continue;
            printf("IRC %s\n", strLine.c_str());

            vector<string> vWords;
            ParseString(strLine, ' ', vWords);
            if (vWords.size() < 2)
                continue;

            char pszName[10000];
            pszName[0] = '\0';

            if (vWords[1] == "352" && vWords.size() >= 8)
            {
                // index 7 is limited to 16 characters
                // could get full length name at index 10, but would be different from join messages
                strcpy(pszName, vWords[7].c_str());
                printf("GOT WHO: [%s]  ", pszName);
            }

            if (vWords[1] == "JOIN" && vWords[0].size() > 1)
            {
                // :username!username@50000007.F000000B.90000002.IP JOIN :#channelname
                strcpy(pszName, vWords[0].c_str() + 1);
                if (strchr(pszName, '!'))
                    *strchr(pszName, '!') = '\0';
                printf("GOT JOIN: [%s]  ", pszName);
            }

            if (pszName[0] == 'u')
            {
                CAddress addr;
                if (DecodeAddress(pszName, addr))
                {
                    CAddrDB addrdb;
                    if (AddAddress(addrdb, addr))
                        printf("new  ");
                    else
                    {
                        // make it try connecting again
                        CRITICAL_BLOCK(cs_mapAddresses)
                            if (mapAddresses.count(addr.GetKey()))
                                mapAddresses[addr.GetKey()].nLastFailed = 0;
                    }
                    addr.print();

                    CRITICAL_BLOCK(cs_mapIRCAddresses)
                        mapIRCAddresses.insert(make_pair(addr.GetKey(), addr));
                }
                else
                {
                    printf("decode failed\n");
                }
            }
        }
        closesocket(hSocket);

        if (GetTime() - nStart > 20 * 60)
        {
            nErrorWait /= 3;
            nRetryWait /= 3;
        }

        nRetryWait = nRetryWait * 11 / 10;
        if (!Wait(nRetryWait += 60))
            return;
    }
}










#ifdef TEST
int main(int argc, char *argv[])
{
    // WSADATA是Windows Sockets API（套接字API）中的一个结构体，
    // 用于存储Windows系统的套接字实现的版本信息。在Windows系统上使用套接字编程时，
    // 通常需要先调用WSAStartup函数来初始化套接字库，
    // 并将返回的WSADATA结构体作为参数传递给该函数。
    // WSADATA结构体包含了套接字库的版本号、支持的网络协议列表、最大数据报文大小等信息
    // ，可以帮助应用程序正确地使用套接字库并兼容不同版本的Windows系统。
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2,2), &wsadata) != NO_ERROR)
    {
        printf("Error at WSAStartup()\n");
        return false;
    }

    ThreadIRCSeed(NULL);

    WSACleanup();
    return 0;
}
#endif
