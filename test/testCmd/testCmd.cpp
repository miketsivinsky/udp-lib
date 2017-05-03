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
const unsigned PassNum = 20;
const unsigned CmdLen  = 14;
const unsigned Port    = 50001;
const char*    Host    = "192.168.10.2";
const char*    Peer    = "192.168.10.1";


//------------------------------------------------------------------------------
static void onTransferReady(const UDP_LIB::TNetAddr& hostAddr, const UDP_LIB::TNetAddr& peerAddr, UDP_LIB::TDirection dir);
static void fillBuf(uint8_t* buf, uint8_t start, unsigned len);

//------------------------------------------------------------------------------
int _tmain(int argc, TCHAR* argv[])
{
	_tprintf(TEXT("[INFO] cmd gen\n"));

	static uint8_t CmdBuf[256];
	const UDP_LIB::TParams txParams = 
	{
		1472,					   // netPacketSize		 - size of 'atomic' UDP packet ('small buffer')
		1,						   // numPacketsInBundle - number of 'atomic' UDP packets in bundle
		64,						   // numBundles         - number of bundles ('big buffers')
		THREAD_PRIORITY_NORMAL,	   // threadPriority	 - priority of internal (UDP_LIB) processing thread 
		INFINITE,                  // timeout            - internal timeot, see TSocket::onExec()
		64*1024,                   // socketBufSize      - SO_RCVBUF/SO_SNDBUF parameter (see MSDN)  
		inet_addr(Peer),           // peerAddr			 - peer (another side) IP address
		Port,                      // peerPort		     - peer (another side) IP port
		onTransferReady            // callback function (notifier), called from TSocket::onExec() at the end of (!) 'sendToReadyQueue'
	};


	UDP_LIB::init();
	UDP_LIB::TStatus status;
	status = UDP_LIB::createSocket(inet_addr(Host),Port,0,&txParams);
	if(status != UDP_LIB::Ok) {
		_tprintf(TEXT("[ERROR] [createSocket] status: %d\n"),status);
		UDP_LIB::cleanUp();
		return 0;
	}
	::Sleep(500);
	_tprintf(TEXT("[INFO] start test\n"));

	//---
	UDP_LIB::Transfer transfer;
	for(unsigned k = 1; k <= PassNum; ++k) {
		_tprintf(TEXT("[INFO] pass: %5d\n"),k);
		status = UDP_LIB::getTransfer(inet_addr(Host),Port,UDP_LIB::Transmit,transfer);
		if(status != UDP_LIB::Ok) {
			_tprintf(TEXT("[ERROR] [getTransfer] status: %d\n"),status);
			break;
		}
		fillBuf(CmdBuf,0,CmdLen);
		transfer.length = CmdLen;
		status = UDP_LIB::submitTransfer(inet_addr(Host),Port,UDP_LIB::Transmit,transfer);
		if(status != UDP_LIB::Ok) {
			_tprintf(TEXT("[ERROR] [submitTransfer] status: %2d\n"),status);
			break;
		}
		::Sleep(2000);
	}

	UDP_LIB::cleanUp();
	_tprintf(TEXT("[INFO] end test\n"));
	
	return 0;
}

//------------------------------------------------------------------------------
void onTransferReady(const UDP_LIB::TNetAddr& /*hostAddr*/, const UDP_LIB::TNetAddr& peerAddr, UDP_LIB::TDirection /*dir*/)
{
	//_tprintf(TEXT("[INFO] callback: IP: %08x, port: %5d\n"),peerAddr.ipAddr, peerAddr.port);
}

//------------------------------------------------------------------------------
void fillBuf(uint8_t* buf, uint8_t start, unsigned len)
{
	while(len--) {
		*buf++ = start++;
	}
}
