
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
const int ProcessPriority    = HIGH_PRIORITY_CLASS;
const int MainThreadPriority = THREAD_PRIORITY_NORMAL;
const unsigned RcvBufSize    = 8*1024*1024;
const unsigned TypeInterval  = 100000;
const unsigned MaxPacketSize = 64*1024;

const unsigned RxTimeout     = 2000;

//------------------------------------------------------------------------------
const unsigned PrgParams = 6;
// argv[0] - program name
// argv[1] - PacketNum
// argv[2] - host IP
// argv[3] - Port
// argv[4] - PacketSize
// argv[5] - PacketGenType
//------------------------------------------------------------------------------
int _tmain(int argc, TCHAR* argv[])
{
	if(argc < PrgParams) {
        	_tprintf(TEXT("No input parameters\n"));
		return 0;
	}

	const unsigned PacketNum = _ttoi(argv[1]);
	char HostAddr[255];
	CharToOem(argv[2],HostAddr);
	const u_short Port       = _ttoi(argv[3]);
	const unsigned PacketSize = _ttoi(argv[4]);
	const unsigned PacketGenType = _ttoi(argv[5]);

	_tprintf(TEXT("--------------------------------------------\n"));
	_tprintf(TEXT("[INFO] [%s start]\n\n"),argv[0]);
	_tprintf(TEXT("[PARAM] PacketNum:     %5d\n"),PacketNum);
	_tprintf(TEXT("[PARAM] PacketSize:    %5d\n"),PacketSize);
	_tprintf(TEXT("[PARAM] host IP:       %s\n"),argv[2]);
	_tprintf(TEXT("[PARAM] port:          %5d\n"),Port);
	_tprintf(TEXT("[PARAM] PacketGenType: %5d\n\n"),PacketGenType);
	_tprintf(TEXT("--------------------------------------------\n"));

	TU::setPriority(ProcessPriority,MainThreadPriority);
	
	//---
	SU::TElapsedTimer timer;
	timer.calibrate();
	int errCode;
	unsigned rvdPacket = 0;
	unsigned errCount = 0;
	unsigned outOfOrderCount = 0;
	uint64_t rvdBytes = 0;

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
			errCode = getsockopt(sock,SOL_SOCKET,SO_RCVBUF,(char*)&optVal,&optLen);
			if(errCode == 0) {
				_tprintf(TEXT("[INFO] SO_RCVBUF:          %8d\n"),optVal);
			}
			errCode = getsockopt(sock,SOL_SOCKET,SO_MAX_MSG_SIZE,(char*)&optVal,&optLen);
			if(errCode == 0) {
				_tprintf(TEXT("[INFO] SO_MAX_MSG_SIZE:    %8d\n"),optVal);
			}
			optVal = RxTimeout;
			setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(char*)&optVal,optLen);
			optVal = RcvBufSize;
			setsockopt(sock,SOL_SOCKET,SO_RCVBUF,(char*)&optVal,optLen);
			errCode = getsockopt(sock,SOL_SOCKET,SO_RCVBUF,(char*)&optVal,&optLen);
			if(errCode == 0) {
				_tprintf(TEXT("[INFO] SO_RCVBUF:          %8d\n"),optVal);
			}
		#endif
		sockaddr_in sockHostAddr;
		sockHostAddr.sin_family      = AF_INET;
		sockHostAddr.sin_addr.s_addr = inet_addr(HostAddr);
		sockHostAddr.sin_port        = htons(Port);

		errCode = bind(sock,(SOCKADDR*)&sockHostAddr,sizeof(sockHostAddr));
		if(errCode != 0) {
			_tprintf(TEXT("[ERROR] [bind] err: %d\n"), errCode);
		}

		uint8_t buf[MaxPacketSize];
		sockaddr_in sockDevAddr;

		_tprintf(TEXT("\n--------------------------------------------\n"));
		uint32_t expectedRvdPacketNum = 0;
		uint32_t rvdPacketNum = 0;
		for(unsigned i = 0; i < PacketNum; ++i) {
			int sockDevAddrLen = sizeof(sockDevAddr);
			int rxLen = recvfrom(sock,(char*)buf,MaxPacketSize,0,(SOCKADDR*)&sockDevAddr,&sockDevAddrLen);
			if(i == 0) {
				timer.start();
			}
			if(rxLen == SOCKET_ERROR) {
				_tprintf(TEXT("[ERROR] [recvfrom] err: %d\n"), WSAGetLastError());
				break;
			} else {
				++rvdPacket;
				rvdBytes += rxLen;
				#if 1
					if(PacketGenType) {
						bool checkOk = TU::checkPacket(buf, rxLen, rvdPacketNum, (PacketGenType > 1));
						if (!checkOk) {
							++errCount;
							_tprintf(TEXT("[ERROR] [recvfrom]  pkt data error: %3d\n"), errCount);
						}
						if (rvdPacketNum != expectedRvdPacketNum) {
							_tprintf(TEXT("[WARN] [recvfrom] pkt expected: %8d, pkt received: %8d\n"), expectedRvdPacketNum, rvdPacketNum);
							expectedRvdPacketNum = rvdPacketNum + 1;
							if (i)
								++outOfOrderCount;
						}
						else {
							++expectedRvdPacketNum;
						}
					}
					if((i % TypeInterval) == 0) {
						_tprintf(TEXT("[INFO] [recvfrom] pkt received: %8d, pkt num: %8d, rxLen: %4d\n"), i, rvdPacketNum, rxLen);
					}
				#endif
			}
		}
        timer.stop();

		//---
		closesocket(sock);
	}

	//---
	errCode = WSACleanup();
	if(errCode != 0) {
		_tprintf(TEXT("[ERROR] [WSACleanup] errCode: %d\n"),errCode);
	}
	_tprintf(TEXT("\n--------------------------------------------\n"));
	_tprintf(TEXT("[INFO] packets expected  %20d\n"),PacketNum);
	_tprintf(TEXT("[INFO] packets received  %20d\n"),rvdPacket);
	_tprintf(TEXT("[INFO] bytes received    %20s\n"),TU::num2str(rvdBytes));
	_tprintf(TEXT("[INFO] corrupted packets %20d\n"),errCount);
	_tprintf(TEXT("[INFO] out of order      %20d\n"),outOfOrderCount);
	_tprintf(TEXT("[INFO] elapsed time      %20s\n"),TU::num2str(timer.getStopDelta()));
	_tprintf(TEXT("[INFO] transfer rate:    %20.1f MB/s\n"),((double)rvdBytes)/timer.getStopDelta());
	_tprintf(TEXT("[INFO] transfer rate:    %20.1f Mb/s\n"),((double)rvdBytes*8)/timer.getStopDelta());
	
	_tprintf(TEXT("\n[INFO] [%s end]\n\n"),argv[0]);
	return 0;
}


