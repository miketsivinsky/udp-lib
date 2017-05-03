
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <cstdio>
#include <tchar.h>
#include <cstdint>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "TU.h"
#include "SU.h"

#pragma comment(lib, "Ws2_32.lib")

//------------------------------------------------------------------------------
// max UDP payload = 1472 (octets)
// UDP overhead    = 66 (octets)
//
// max relative speed (for UDP palyload = 1472) = 0.9571
// max relative speed (for UDP palyload = 1024) = 0.9394
//
// process priority classes:
//		REALTIME_PRIORITY_CLASS     0x00000100
//		HIGH_PRIORITY_CLASS         0x00000080
//		ABOVE_NORMAL_PRIORITY_CLASS 0x00008000
//		NORMAL_PRIORITY_CLASS       0x00000020
//		BELOW_NORMAL_PRIORITY_CLASS 0x00004000
//		IDLE_PRIORITY_CLASS         0x00000040
//
// thread priority classes:
//		THREAD_PRIORITY_TIME_CRITICAL 15
//		THREAD_PRIORITY_HIGHEST       2
//		THREAD_PRIORITY_ABOVE_NORMAL  1
//		THREAD_PRIORITY_NORMAL        0
//		THREAD_PRIORITY_BELOW_NORMAL -1
//		THREAD_PRIORITY_LOWEST       -2
//		THREAD_PRIORITY_IDLE        -15
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
const int ProcessPriority    = NORMAL_PRIORITY_CLASS;
const int MainThreadPriority = THREAD_PRIORITY_NORMAL;
const unsigned TxBufSize     = 8*1024*1024;
const unsigned TypeInterval  = 100000;
const unsigned MaxPacketSize = 64*1024;

const unsigned TxDelayPeriod = 100; // packets between two delays

//------------------------------------------------------------------------------
const unsigned PrgParams = 8;
// argv[0] - program name
// argv[1] - PacketNum
// argv[2] - host IP
// argv[3] - Port
// argv[4] - PacketSize
// argv[5] - PacketGenType
// argv[6] - TxDelay
// argv[7] - device IP 
//------------------------------------------------------------------------------
int _tmain(int argc, TCHAR* argv[])
{
	if(argc < PrgParams) {
        	_tprintf(TEXT("No input parameters\n"));
	        return 0;
	}
    
	const unsigned PacketNum     = _ttoi(argv[1]);
	char HostAddr[255]; CharToOem(argv[2],HostAddr);
	const u_short Port           = _ttoi(argv[3]);
	const unsigned PacketSize    = _ttoi(argv[4]);
	const unsigned PacketGenType = _ttoi(argv[5]);
	const unsigned TxDelay       = _ttoi(argv[6]);
	char DevAddr[255]; CharToOem(argv[7],DevAddr);

	_tprintf(TEXT("--------------------------------------------\n"));
	_tprintf(TEXT("[INFO] [%s start]\n\n"),argv[0]);
	_tprintf(TEXT("[PARAM] PacketNum:     %5d\n"),PacketNum);
	_tprintf(TEXT("[PARAM] PacketSize:    %5d\n"),PacketSize);
	_tprintf(TEXT("[PARAM] TxDelay:       %5d\n"),TxDelay);
	_tprintf(TEXT("[PARAM] host IP:       %s\n"),argv[2]);
	_tprintf(TEXT("[PARAM] port:          %5d\n"),Port);
	_tprintf(TEXT("[PARAM] PacketGenType: %5d\n\n"),PacketGenType);
	_tprintf(TEXT("[PARAM] device IP:     %s\n"),argv[7]);
	_tprintf(TEXT("--------------------------------------------\n"));

	TU::setPriority(ProcessPriority,MainThreadPriority);

	//---
	SU::TElapsedTimer timer;
	timer.calibrate();
	int errCode;
	unsigned sentPacket = 0;
	uint64_t sentBytes = 0;

	WORD vVersionRequested = MAKEWORD(2,2);
	WSADATA wsaData;
	errCode = WSAStartup(vVersionRequested, &wsaData);
	if(errCode != 0) {
		_tprintf(TEXT("[ERROR] [WSAStartup] errCode: %d\n"),errCode);
		return 1;
	}

	//---
	_tprintf(TEXT("[INFO] wVersion:           0x%04x\n"),wsaData.wVersion);
	_tprintf(TEXT("[INFO] wHighVersion:       0x%04x\n"),wsaData.wHighVersion);
	printf("[INFO] szDescription:      %s\n",wsaData.szDescription);
	printf("[INFO] szSystemStatus:     %s\n",wsaData.szSystemStatus);

	//---
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sock == INVALID_SOCKET) {
		_tprintf(TEXT("[ERROR] [socket] err: %d\n") , WSAGetLastError());
	} else {
		//---
		#if 1
			int optVal;
			int optLen = sizeof(optVal);
			errCode = getsockopt(sock,SOL_SOCKET,SO_SNDBUF,(char*)&optVal,&optLen);
			if(errCode == 0) {
				_tprintf(TEXT("[INFO] SO_SNDBUF:          %8d\n"),optVal);
			}
			optVal = TxBufSize;
			setsockopt(sock,SOL_SOCKET,SO_SNDBUF,(char*)&optVal,optLen);
			errCode = getsockopt(sock,SOL_SOCKET,SO_SNDBUF,(char*)&optVal,&optLen);
			if(errCode == 0) {
				_tprintf(TEXT("[INFO] SO_SNDBUF:          %8d\n"),optVal);
			}
		#endif
		//---
		sockaddr_in sockDevAddr;
		sockDevAddr.sin_family      = AF_INET;
		InetPton(AF_INET, argv[7], &sockDevAddr.sin_addr.s_addr);
		//sockDevAddr.sin_addr.s_addr = inet_addr(DevAddr);
		//printf("[DEBUG] DevtAddr: 0x%08x\n", sockDevAddr.sin_addr.s_addr);
		sockDevAddr.sin_port        = htons(Port);

		#if 1
			errCode = bind(sock,(SOCKADDR*)&sockDevAddr,sizeof(sockDevAddr));
			if(errCode != 0) {
				_tprintf(TEXT("[ERROR] [bind] err: %d, error code: %u\n"), errCode,WSAGetLastError());
			}
		#endif

		sockaddr_in sockHostAddr;
		sockHostAddr.sin_family      = AF_INET;
		InetPton(AF_INET, argv[2], &sockHostAddr.sin_addr.s_addr);
		//sockHostAddr.sin_addr.s_addr = inet_addr(HostAddr);
		//printf("[DEBUG] HostAddr: 0x%08x\n", sockHostAddr.sin_addr.s_addr);

		sockHostAddr.sin_port        = htons(Port);
		uint8_t buf[MaxPacketSize];
		uint32_t value = 0;

		_tprintf(TEXT("\n--------------------------------------------\n"));
		Sleep(200);
 		timer.start();
		for(unsigned i = 0; i < PacketNum; ++i) {
			if(PacketGenType) {
				TU::packetGen(buf, PacketSize, i, (PacketGenType > 1));
			}
			int txLen = sendto(sock,(const char*)buf,PacketSize,0,(SOCKADDR*)&sockHostAddr,sizeof(sockHostAddr));
			if((txLen == SOCKET_ERROR) || (txLen != PacketSize)) {
				_tprintf(TEXT("[ERROR] [sendto] err: %d, txLen: %4d\n"), WSAGetLastError(),txLen);
				break;
			} else {
				++sentPacket;
				sentBytes += txLen;
				if(TxDelay) {
					if((i%TxDelayPeriod) == 0) {
						Sleep(TxDelay);
					}
				}
				if((i%TypeInterval) == 0) {
					_tprintf(TEXT("[INFO] [sendto]  packet sent: %8d\n"),i);
				}
			}
		}
		timer.stop();

		//---
		Sleep(100);
		closesocket(sock);
	}

	//---
	errCode = WSACleanup();
	if(errCode != 0) {
		_tprintf(TEXT("[ERROR] [WSACleanup] errCode: %d\n"),errCode);
	}
	_tprintf(TEXT("\n--------------------------------------------\n"));
	_tprintf(TEXT("[INFO] packets sent    %20d\n"),sentPacket);
	_tprintf(TEXT("[INFO] bytes sent      %20s\n"),TU::num2str(sentBytes));
	_tprintf(TEXT("[INFO] elapsed time    %20s\n"),TU::num2str(timer.getStopDelta()));
	_tprintf(TEXT("[INFO] transfer rate:  %20.1f MB/s\n"),((double)sentBytes)/timer.getStopDelta());
	_tprintf(TEXT("[INFO] transfer rate:  %20.1f Mb/s\n"),((double)sentBytes*8)/timer.getStopDelta());

	_tprintf(TEXT("\n[INFO] [%s end]\n\n"),argv[0]);
	return 0;
}


