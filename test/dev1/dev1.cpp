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

const int ProcessPriority      = HIGH_PRIORITY_CLASS;
const int MainThreadPriority   = THREAD_PRIORITY_HIGHEST;  
const int WorkerThreadPriority = THREAD_PRIORITY_NORMAL;  

const unsigned BundleNum    = 128;
const unsigned Timeout      = 2000;
const int SocketBufSize     = 8*1024*1024;
const unsigned TypeInterval = 10000;

//------------------------------------------------------------------------------
const unsigned PrgParams = 9;
// argv[0] - program name
// argv[1] - PacketNum
// argv[2] - host IP
// argv[3] - Port
// argv[4] - PacketSize
// argv[5] - PacketsInBuf
// argv[6] - PacketGenType
// argv[7] - TxDelay
// argv[8] - device IP 
//------------------------------------------------------------------------------

static bool typeInfo(unsigned nBuf, UDP_LIB::TStatus status, UDP_LIB::Transfer& transfer);
static void printTransferInfo(const UDP_LIB::Transfer* transfer);
static void onTransferReady(const UDP_LIB::TNetAddr& hostAddr, const UDP_LIB::TNetAddr& peerAddr, UDP_LIB::TDirection dir);

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
	const unsigned TxDelay          = _ttoi(argv[7]);
	char PeerAddrStr[255]; CharToOem(argv[8],PeerAddrStr);

	//---
	_tprintf(TEXT("--------------------------------------------\n"));
	_tprintf(TEXT("[INFO] [%s start]\n\n"),argv[0]);
	_tprintf(TEXT("[PARAM] BufNum:     %8d\n"),BufNum);
	_tprintf(TEXT("[PARAM] PacketSize:    %5d\n"),PacketSize);
	_tprintf(TEXT("[PARAM] PacketsInBuf:  %5d\n"),PacketsInBuf);
	_tprintf(TEXT("[PARAM] TxDelay:       %5d\n"),TxDelay);
	_tprintf(TEXT("[PARAM] host IP:       %s\n"),argv[2]);
	_tprintf(TEXT("[PARAM] port:          %5d\n"),Port);
	_tprintf(TEXT("[PARAM] PacketGenType: %5d\n"),PacketGenType);
	_tprintf(TEXT("[PARAM] peer IP:       %s\n"),argv[8]);
	_tprintf(TEXT("--------------------------------------------\n"));

	//---
	const unsigned long HostAddr = inet_addr(HostAddrStr);
	const unsigned long PeerAddr = inet_addr(PeerAddrStr);
	const UDP_LIB::TParams txParams     = { PacketSize, PacketsInBuf, BundleNum, WorkerThreadPriority, Timeout, SocketBufSize, PeerAddr, Port, 0/*onTransferReady*/ }; 
	const UDP_LIB::TDirection Direction = UDP_LIB::Transmit;

	//---
	UDP_LIB::TStatus status;
	unsigned sentBuf = 0;
	uint64_t sentBytes = 0;
	SU::TElapsedTimer timer;
	timer.calibrate();

	//---
	TU::setPriority(ProcessPriority,MainThreadPriority);

	UDP_LIB::init();
	status = UDP_LIB::createSocket(HostAddr,Port,0,&txParams);
	if(status != UDP_LIB::Ok) {
		_tprintf(TEXT("[ERROR][MAIN] [createSocket] status: %d\n"),status);
		UDP_LIB::cleanUp();
		return 0;
	}

	//---
	UDP_LIB::Transfer transfer;
	#if defined(TEST_SEQUENCE_GEN)
		TU::TStreamTester streamGen;
	#endif

	//**************************************************************
	_tprintf(TEXT("\n----------------------------------------------------\n"));
	::Sleep(500);
	timer.start();
	for(unsigned nBuf = 0; nBuf < BufNum; ++nBuf) {
		status = UDP_LIB::getTransfer(HostAddr,Port,Direction,transfer);
		if(!typeInfo(nBuf,status,transfer))
			continue;
		if(PacketGenType) {
			#if !defined(TEST_SEQUENCE_GEN)
				TU::packetGen(transfer.buf, transfer.bufLength, nBuf, (PacketGenType > 1),PacketsInBuf);
			#else
				streamGen.fillBuf(transfer.buf,transfer.bufLength);
			#endif
		}
		transfer.length = transfer.bufLength; 
		status = UDP_LIB::submitTransfer(HostAddr,Port,Direction,transfer);
		if(status != UDP_LIB::Ok) {
			_tprintf(TEXT("[ERROR][MAIN] [submitTransfer] [%8d]: status: %2d\n"),nBuf,status);
			break;
		}
		++sentBuf;
		sentBytes += transfer.length;
		if(TxDelay) {
			::Sleep(TxDelay);
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
	_tprintf(TEXT("[INFO] buffers sent    %20d\n"),sentBuf);
	_tprintf(TEXT("[INFO] bytes sent      %20s\n"),TU::num2str(sentBytes));
	_tprintf(TEXT("[INFO] elapsed time    %20s\n"),TU::num2str(timer.getStopDelta()));
	_tprintf(TEXT("[INFO] transfer rate:  %20.1f MB/s\n"),((double)sentBytes)/timer.getStopDelta());
	_tprintf(TEXT("[INFO] transfer rate:  %20.1f Mb/s\n"),((double)sentBytes*8)/timer.getStopDelta());
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
void printTransferInfo(const UDP_LIB::Transfer* transfer)
{
	_tprintf(TEXT("--- transfer ---\n"));
	_tprintf(TEXT("bundleId:  %8d\n"),transfer->bundleId);
	_tprintf(TEXT("direction: %8d\n"),transfer->direction);
	_tprintf(TEXT("status:    %8d\n"),transfer->status);
	_tprintf(TEXT("length:    %8d\n"),transfer->length);
	_tprintf(TEXT("bufLength: %8d\n"),transfer->bufLength);
	_tprintf(TEXT("buf:       %8x\n"),(unsigned)(transfer->buf));
	_tprintf(TEXT("isStream:  %8d\n"),transfer->isStream);
}

//------------------------------------------------------------------------------
void onTransferReady(const UDP_LIB::TNetAddr& hostAddr, const UDP_LIB::TNetAddr& peerAddr, UDP_LIB::TDirection dir)
{
	_tprintf(TEXT("[onTransferReady] hostAddr: %08x, hostPort: %5d, peerAddr: %08x, peerPort: %5d, dir: %1d\n"),hostAddr.ipAddr,hostAddr.port,peerAddr.ipAddr,peerAddr.port,dir);
}