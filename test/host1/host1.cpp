//------------------------------------------------------------------------------
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <cstdio>
#include <tchar.h>
#include <cstdint>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "TU.h"
#include "SU.h"
#include "UDP_Lib.h"

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
#define TEST_SEQUENCE_GEN

const int ProcessPriority      = REALTIME_PRIORITY_CLASS;
const int MainThreadPriority   = THREAD_PRIORITY_HIGHEST;
const int WorkerThreadPriority = THREAD_PRIORITY_NORMAL;  

const unsigned BundleNum = 128;
const unsigned Timeout   = 2000;
const int SocketBufSize  = 8*1024*1024;

const unsigned TypeInterval = 10000;

//------------------------------------------------------------------------------
const unsigned PrgParams = 7;
// argv[0] - program name
// argv[1] - PacketNum
// argv[2] - host IP
// argv[3] - Port
// argv[4] - PacketSize
// argv[5] - PacketsInBuf
// argv[6] - PacketGenType
//------------------------------------------------------------------------------

static bool typeInfo(unsigned nBuf, UDP_LIB::TStatus status, UDP_LIB::Transfer& transfer);
static void checkBuf(unsigned PacketGenType, unsigned& errCount, unsigned& outOfOrderCount, UDP_LIB::Transfer& transfer, unsigned packetsInBuf);

//------------------------------------------------------------------------------
int _tmain(int argc, TCHAR* argv[])
{
	if(argc < PrgParams) {
        _tprintf(TEXT("No input parameters\n"));
        return 0;
	}

	//---
	const unsigned BufNum           = _ttoi(argv[1]);
	char HostAddrStr[255]; CharToOem(argv[2],HostAddrStr);
	const u_short Port              = _ttoi(argv[3]);
	const unsigned PacketSize       = _ttoi(argv[4]);
	const unsigned PacketsInBuf     = _ttoi(argv[5]);
	const unsigned PacketGenType    = _ttoi(argv[6]);

	//---
	_tprintf(TEXT("--------------------------------------------\n"));
	_tprintf(TEXT("[INFO] [%s start]\n\n"),argv[0]);
	_tprintf(TEXT("[PARAM] BufNum:     %8d\n"),BufNum);
	_tprintf(TEXT("[PARAM] PacketSize:    %5d\n"),PacketSize);
	_tprintf(TEXT("[PARAM] PacketsInBuf:  %5d\n"),PacketsInBuf);
	_tprintf(TEXT("[PARAM] host IP:       %s\n"),argv[2]);
	_tprintf(TEXT("[PARAM] port:          %5d\n"),Port);
	_tprintf(TEXT("[PARAM] PacketGenType: %5d\n"),PacketGenType);
	_tprintf(TEXT("--------------------------------------------\n"));

	//---
	unsigned long HostAddr;

	InetPton(AF_INET, argv[2], &HostAddr);
	const UDP_LIB::TParams rxParams     = { PacketSize, PacketsInBuf, BundleNum, WorkerThreadPriority, Timeout, SocketBufSize, 0, 0, 0 }; 
	const UDP_LIB::TDirection Direction = UDP_LIB::Receive;

	//---
	UDP_LIB::TStatus status;
	unsigned rvdBuf = 0;
	uint64_t rvdBytes = 0;
	SU::TElapsedTimer timer;
	timer.calibrate();

	//---
	TU::setPriority(ProcessPriority,MainThreadPriority);

	UDP_LIB::init();
	status = UDP_LIB::createSocket(HostAddr,Port,&rxParams,0);
	if(status != UDP_LIB::Ok) {
		_tprintf(TEXT("[ERROR][MAIN] [createSocket] status: %d\n"),status);
		UDP_LIB::cleanUp();
		return 0;
	}

	//---
	UDP_LIB::Transfer transfer;
	uint32_t rvdBufNum = 0;
	uint32_t expectedRvdBufNum = 0;
	unsigned errCount = 0;
	unsigned outOfOrderCount = 0;
	#if defined (TEST_SEQUENCE_GEN)
		TU::TStreamTester streamChecker;
	#endif


	//**************************************************************
	_tprintf(TEXT("\n----------------------------------------------------\n"));
	for(unsigned nBuf = 0; nBuf < BufNum; ++nBuf) {
		status = UDP_LIB::getTransfer(HostAddr, Port, Direction, transfer);
		if(!typeInfo(nBuf,status,transfer))
			continue;
		if(nBuf == 0) {
			timer.start();
		}
		++rvdBuf;
		rvdBytes += transfer.length;

		#if 0 
			uint32_t* bufNum = (uint32_t*)transfer.buf;
			printf("bufNum: %8d",*bufNum);
			for(int k = 0; k < 8; ++k) {
				printf("%2x",transfer.buf[4+k]);
			}
			printf("\n");
		#endif
		
			if (PacketGenType) {
				#if !defined (TEST_SEQUENCE_GEN)
					/*CHECK BUF*/ checkBuf(PacketGenType,errCount,outOfOrderCount,transfer,PacketsInBuf);
				#else
					unsigned errInCurrBuf = streamChecker.checkBuf(transfer.buf, transfer.length);
					errCount += errInCurrBuf;
					if (errInCurrBuf) {
						_tprintf(TEXT("[ERROR] %6d errors in received buf\n"), errInCurrBuf);
					}
				#endif
			}
		status = UDP_LIB::submitTransfer(HostAddr,Port,Direction,transfer);
		if(status != UDP_LIB::Ok) {
			_tprintf(TEXT("[ERROR][MAIN] [submitTransfer] [%8d]: status: %2d\n"),nBuf,status);
			break;
		}
	}
	timer.stop();
	_tprintf(TEXT("\n----------------------------------------------------\n"));
	//**************************************************************

	//---
	::Sleep(500);
	UDP_LIB::cleanUp();

	//---
	_tprintf(TEXT("\n--------------------------------------------\n"));
	_tprintf(TEXT("[INFO] buffers expected %20d\n"),BufNum);
	_tprintf(TEXT("[INFO] buffers received %20d\n"),rvdBuf);
	_tprintf(TEXT("[INFO] bytes received   %20s\n"),TU::num2str(rvdBytes));
	_tprintf(TEXT("[INFO] corrupted bufs   %20d\n"),errCount);
	_tprintf(TEXT("[INFO] out of order     %20d\n"),outOfOrderCount);
	_tprintf(TEXT("[INFO] elapsed time     %20s\n"),TU::num2str(timer.getStopDelta()));
	_tprintf(TEXT("[INFO] transfer rate:   %20.1f MB/s\n"),((double)rvdBytes)/timer.getStopDelta());
	_tprintf(TEXT("[INFO] transfer rate:   %20.1f Mb/s\n"),((double)rvdBytes*8)/timer.getStopDelta());
	_tprintf(TEXT("\n"));

	return 0;
}

//------------------------------------------------------------------------------
bool typeInfo(unsigned nBuf, UDP_LIB::TStatus status, UDP_LIB::Transfer& transfer)
{
	if(status != UDP_LIB::Ok) {
		_tprintf(TEXT("[ERROR][MAIN] [getTransfer] [%8d]: status: %2d, trStat: %2d\n"),nBuf,status,transfer.status);
		return false;
	}
	if(transfer.status != UDP_LIB::Ok) {
		_tprintf(TEXT("[WARN][MAIN] [getTransfer] [%8d]: bufId: %3d, trStat: %2d\n"),nBuf,transfer.bundleId,transfer.status);
	}

	bool enaTypeInfo = ((nBuf % TypeInterval) == 0);
	if(enaTypeInfo) {
			_tprintf(TEXT("[INFO][MAIN] [getTransfer] [%8d]: bufId: %3d, dir: %1d, trStat: %2d, len: %5d, bufLen: %5d, isStream: %1d\n"),
				                                  nBuf,
												  transfer.bundleId,
												  transfer.direction,
												  transfer.status,
												  transfer.length,
												  transfer.bufLength,
												  transfer.isStream
					);
	}
	return true;
}

//------------------------------------------------------------------------------
void checkBuf(unsigned PacketGenType, unsigned& errCount, unsigned& outOfOrderCount, UDP_LIB::Transfer& transfer, unsigned packetsInBuf)
{
	static bool fStart = true;
	static uint32_t rvdPacketNum = 0;
	static uint32_t expectedRvdPacketNum = 0;

	if(PacketGenType) {
		uint8_t* buf = transfer.buf;
		unsigned packetSize = transfer.length/packetsInBuf;

		for(unsigned pkt = 0; pkt < packetsInBuf; ++pkt) {
			bool packetValid = TU::checkPacket(buf,packetSize,rvdPacketNum,(PacketGenType > 1));
			if(!packetValid) {
				++errCount;
				_tprintf(TEXT("[ERROR][MAIN] packet data error: %4d\n"),errCount);
			}
			if(rvdPacketNum != expectedRvdPacketNum) {
				_tprintf(TEXT("[ERROR][MAIN] packet expected: %8u, packet received: %8u\n"),expectedRvdPacketNum,rvdPacketNum);
				expectedRvdPacketNum = rvdPacketNum + 1;
				if(fStart) {
					fStart = false;
				} else {
					++outOfOrderCount;
				}
			} else {
				++expectedRvdPacketNum;;
			}

			buf = buf + packetSize;
		}
	}
}


#if 0
//------------------------------------------------------------------------------
//--- TEST
	DWORD processMask, systemMask;
	::GetProcessAffinityMask(::GetCurrentProcess(),&processMask,&systemMask);
	_tprintf(TEXT("System Affinity Mask: %x, Process Affinity Mask: %x\n"),systemMask,processMask);
	//::SetThreadAffinityMask(::GetCurrentThread(),0x01);
	_tprintf(TEXT("[****] WSA_MAXIMUM_WAIT_EVENTS = %d\n"),WSA_MAXIMUM_WAIT_EVENTS);
	//--- end (TEST)
//------------------------------------------------------------------------------
#endif