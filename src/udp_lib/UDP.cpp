//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include "UDP.h"

#if defined(UDP_PRINT_DEBUG_INFO)
	#include <Ws2tcpip.h>
#endif

#pragma comment(lib, "Ws2_32.lib")

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
TSocketPacket::TSocketPacket(unsigned packetId, unsigned bundleId, char* buf, u_long bufLen) :
																								mPacketId(packetId),
																								mBundleId(bundleId),
																								mEvent(false)
{
	mBuf.buf = buf;
	mBuf.len = bufLen;

	//mEvent.reset();
	WSAOVERLAPPED::hEvent = mEvent.getHandle();
	WSAOVERLAPPED::Offset = 0;
	WSAOVERLAPPED::OffsetHigh = 0;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
TSocketPacketBundle::TSocketPacketBundle(unsigned bundleId, unsigned numPacketsInBundle, unsigned netPacketSize) :
																								mBundleId(bundleId),
																								mNumPacketsInBundle(numPacketsInBundle),
																								mBuf(0),
																								mPacketBundle(0),
																								mEvents(0)
{
	mBuf = new uint8_t [netPacketSize*mNumPacketsInBundle];
	mEvents = new HANDLE [mNumPacketsInBundle];
	mPacketBundle = new TSocketPacket* [mNumPacketsInBundle];

	for(unsigned id = 0; id < mNumPacketsInBundle; ++id) {
		mPacketBundle[id] = new TSocketPacket(id, mBundleId, reinterpret_cast<char*>(mBuf + netPacketSize*id),netPacketSize);
		mEvents[id] = mPacketBundle[id]->getEvent();
	}
}

//------------------------------------------------------------------------------
TSocketPacketBundle::~TSocketPacketBundle()
{
	if(mPacketBundle) {
		for(unsigned id = 0; id < mNumPacketsInBundle; ++id) {
			delete mPacketBundle[id];
		}
		delete [] mPacketBundle;
	}
	delete [] mBuf;
	delete [] mEvents;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
bool TSocket::TTransferStatus::setStatus(int status)
{
	if(status == 0) {
		++nCompleted;
		return true;
	}
	if(::WSAGetLastError() == WSA_IO_PENDING) {
		++nPending;
		return true;
	} else {
		++nError;
		return false;
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
TSocket::TSocket(TSocketWrapper* wrapper, const TCHAR* threadName, const UDP_LIB::TParams& params) :
																	SU::TThreadExt(threadName),
																	mParams(params),
																	mWrapper(wrapper),
																	mDrvSem(0,params.numBundles),
																	mAppSem(0,params.numBundles),
																	mBundleArray(0),
																	mSubmitQueue(),
																	mReadyQueue()
																	
{
	setThreadPriority(mParams.threadPriority);
	//::SetThreadAffinityMask(getHandle(),0x01);

	if(mParams.numPacketsInBundle > WSA_MAXIMUM_WAIT_EVENTS) {
		mParams.numPacketsInBundle = WSA_MAXIMUM_WAIT_EVENTS;
	}

	mBundleArray = new TSocketPacketBundle* [mParams.numBundles];
	for(unsigned id = 0; id < mParams.numBundles; ++id) {
		mBundleArray[id] = new TSocketPacketBundle(id, mParams.numPacketsInBundle, mParams.netPacketSize);
	}
	mPeerAddr.sin_family      = AF_INET;
	mPeerAddr.sin_addr.s_addr = mParams.peerAddr;
	mPeerAddr.sin_port        = htons(mParams.peerPort);

	#if defined(UDP_PRINT_DEBUG_INFO)
		const unsigned BufSize = 128;
		TCHAR strBuf[BufSize];

		u_char s_b1 = mPeerAddr.sin_addr.S_un.S_un_b.s_b1;
		u_char s_b2 = mPeerAddr.sin_addr.S_un.S_un_b.s_b2;
		u_char s_b3 = mPeerAddr.sin_addr.S_un.S_un_b.s_b3;
		u_char s_b4 = mPeerAddr.sin_addr.S_un.S_un_b.s_b4;

		_stprintf_s(strBuf, BufSize, TEXT("%03d.%03d.%03d.%03d:%05d"), s_b1, s_b2, s_b3, s_b4, mParams.peerPort);
	#endif

	#if defined(UDP_PRINT_DEBUG_INFO)
		_tprintf(TEXT("[INFO] [TSocket] %s {Peer: %s}, PacketBundles: %3d, PacketsInBundle: %2d, PacketSize: %4d\n"),threadName,strBuf,mParams.numBundles, mParams.numPacketsInBundle,mParams.netPacketSize);
	#endif
}

//------------------------------------------------------------------------------
TSocket::~TSocket()
{
	//---
	//waitEndOfThread();

	//---
	if(mBundleArray) {
		for(unsigned id = 0; id < mParams.numBundles; ++id) {
			delete mBundleArray[id];
		}
		delete [] mBundleArray;
	}

	#if defined(UDP_PRINT_DEBUG_INFO)
		_tprintf(TEXT("[INFO] [~TSocket] %s\n"),getThreadName());
	#endif
}

//------------------------------------------------------------------------------
bool TSocket::clearTransfer(const TSocket::TTransferInfo& transferInfo)
{
	bool res = true;
	#if defined(CANCEL_IO_EX_SUPPORTED)
		for(unsigned packetId = 0; packetId < mParams.numPacketsInBundle; ++packetId) {
			if(!::CancelIoEx((HANDLE)getWrapper()->getSocket(), getPacket(transferInfo.transferId, packetId)) && (::GetLastError() != ERROR_NOT_FOUND)) {
				res = false;
				#if defined(UDP_PRINT_DEBUG_ERROR)
					_tprintf(TEXT("[ERROR] ::CancelIoEx, packetId: %3d, err: %d\n"),packetId, ::GetLastError());
				#endif
			}
		}
	#else
		::CancelIo((HANDLE)getWrapper()->getSocket());	
	#endif
	return res;
}

//------------------------------------------------------------------------------
void TSocket::onExec()
{
	TTransferInfo transferInfo;
	
	//---
	mDrvSem.wait(INFINITE);
	if(mSubmitQueue.get(transferInfo)) {
		transferInfo.status = UDP_LIB::Ok; // TODO: check status when get from mSubmitQueue
		#if defined(UDP_PRINT_DEBUG_INFO)
			//_tprintf(TEXT("[INFO] [onExec] %s, pri: %2d, transferId: %2d\n"),getThreadName(),getThreadPriority(),transferInfo.transferId);
		#endif
		//---
		if ((getDir() == UDP_LIB::Transmit) && (transferInfo.length == 0)) {
			sendToReadyQueue(transferInfo);
			return;
		}

		//---
		int xmitLength = 0;
		DWORD status = ::WSAWaitForMultipleEvents(
												  mParams.numPacketsInBundle,             // cEvents
												  getEventArray(transferInfo.transferId), // event array
												  TRUE,                                   // fWaitAll
												  mParams.timeout,                        // dwTimeout
												  FALSE                                   // fAlertable
			                                     );


		switch (status) {
			//---
			case WSA_WAIT_EVENT_0:
				//---
				{
					BOOL ovlRes;
					DWORD bytesTransferred;
					DWORD flags;
					WSABUF* buf;
					bool changeXmitLenNotAllowed = ((mParams.numPacketsInBundle != 1) || (getDir() != UDP_LIB::Receive));

					//---
					for(unsigned packetId = 0; packetId < mParams.numPacketsInBundle; ++packetId) {
						ovlRes = ::WSAGetOverlappedResult(
															getWrapper()->getSocket(),                   // socket
															getPacket(transferInfo.transferId,packetId), // overlapped
															&bytesTransferred,                           // bytes transferred
															FALSE,                                       // fWait
															&flags                                       // flags
												          );
						xmitLength += bytesTransferred;
						#if 1
						if((ovlRes != TRUE) && (transferInfo.status != UDP_LIB::XmitLenError)) {
							transferInfo.status = UDP_LIB::WSAOvlError;
						}
						buf = getBuf(transferInfo.transferId,packetId);
						if((bytesTransferred != buf->len) && changeXmitLenNotAllowed) {
							transferInfo.status = UDP_LIB::XmitLenError;
						}
						#endif
						#if 0
							if((getDir() == UDP_LIB::Receive) && !ovlRes) {
								printf("[slon 1] [%3d] err: %d, bytes: %5d\n",packetId, ::GetLastError(),bytesTransferred);
							}
						#endif
					}
				}
				break;
			//---
			case WSA_WAIT_TIMEOUT:
				clearTransfer(transferInfo);
				if(getDir() == UDP_LIB::Receive) {
					#if 1
						submitTransfer(transferInfo);
						return;
					#else
						transferInfo.status = UDP_LIB::SocketWaitTimeout;
					#endif
				} else {
					transferInfo.status = UDP_LIB::SocketWaitTimeout;
				}
				break;
			//---
			default: // TODO: check for WSA_WAIT_FAILED and then call WSAGetLastError()
				clearTransfer(transferInfo);
				transferInfo.status = UDP_LIB::WSAWaitError;
				break;
		};

		//---
		transferInfo.length = xmitLength;
		sendToReadyQueue(transferInfo);
		return;
	} 
	#if defined(UDP_PRINT_DEBUG_ERROR)
		_tprintf(TEXT("[ERROR] [onExec] thread: %s, empty queue\n"),getThreadName());
	#endif
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocket::submitTransfer(UDP_LIB::Transfer& transfer)
{
	if(transfer.direction != getDir()) {
		return UDP_LIB::SubmitError;
	}

	//---
	TTransferInfo transferInfo(transfer.bundleId,transfer.status,transfer.length);
	#if 0 // clear transfer fields after submitTransfer
		transfer.bundleId = -1;
		transfer.direction = UDP_LIB::NotInit;
		transfer.length = 0;
		transfer.status = UDP_LIB::NotInitialized;
	#endif

	return submitTransfer(transferInfo);
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocket::submitTransfer(const TTransferInfo& transferInfo)
{
	//---
	if((transferInfo.transferId < 0) || (transferInfo.transferId >= static_cast<int>(mParams.numBundles))) {
		return UDP_LIB::SubmitError;
	}

	//---
	TTransferStatus transferStatus;
	WSABUF* buf;
	WSAOVERLAPPED* ovl;
	bool changeXmitLen = (mParams.numPacketsInBundle == 1) && (getDir() == UDP_LIB::Transmit) && (transferInfo.length > 0);

	//---
	if ((getDir() == UDP_LIB::Transmit) && (transferInfo.length == 0)) {
		sendToSubmitQueue(transferInfo);
		return UDP_LIB::Ok;
	}

	//---
	for(unsigned packetId = 0; packetId < mParams.numPacketsInBundle; ++packetId) {
		buf = getBuf(transferInfo.transferId,packetId);
		ovl = getPacket(transferInfo.transferId,packetId);
		buf->len = changeXmitLen ? transferInfo.length : mParams.netPacketSize;
		#if defined(UDP_DEBUG_CHECK_PARAMS)
			testSocketXmitParams(ovl,buf,packetId);
		#endif
		int status = socketTransfer(buf,ovl);
		transferStatus.setStatus(status);
	}

	#if defined(UDP_PRINT_DEBUG_ERROR)
		if(transferStatus.nError) {
			_tprintf(TEXT("[ERROR] [TSocket::socketTransfer] %3d errors after %3d transfer requests\n"),transferStatus.nError,mParams.numPacketsInBundle);
		}
	#endif
	#if defined(UDP_PRINT_DEBUG_INFO)
		if (transferStatus.nCompleted) {
			//printf("[WARN] [submitTransfer] transfers completed %d\n", transferStatus.nCompleted);
		}
		//_tprintf(TEXT("[INFO] [submitTransfer] completed: %2d, pending: %2d, errors: %2d\n"),transferStatus.nCompleted,transferStatus.nPending,transferStatus.nError);
	#endif

	//---
	sendToSubmitQueue(transferInfo);
	return UDP_LIB::Ok;
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocket::getTransfer(UDP_LIB::Transfer& transfer, DWORD timeout)
{
	TTransferInfo transferInfo;
	UDP_LIB::TStatus status = getTransfer(transferInfo,timeout);
	if(status == UDP_LIB::Ok) {
		transfer.bundleId = transferInfo.transferId;
		transfer.direction = getDir();
		transfer.buf = getBuf(transferInfo.transferId);
		transfer.length = transferInfo.length;
		transfer.bufLength = getBufLength();
		transfer.isStream  = isStream();
	}
	transfer.status = transferInfo.status;
	return status;
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocket::getTransfer(TTransferInfo& transferInfo, DWORD timeout)
{
	DWORD semStatus = mAppSem.wait(timeout);
	if(semStatus == WAIT_OBJECT_0) {
		if(mReadyQueue.get(transferInfo)) {
			return UDP_LIB::Ok;
		} else {
			transferInfo.status = UDP_LIB::SocketTransferError;
		}
	} else {
		transferInfo.status = UDP_LIB::SocketWaitTimeout;
	}
	return transferInfo.status;
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocket::tryGetTransfer(UDP_LIB::Transfer& transfer)
{
	if(!getReadyTransferNum())
		return  UDP_LIB::NoReadyTransfers;
	return getTransfer(transfer,0);
}

//------------------------------------------------------------------------------
void TSocket::start()
{
	resumeThread();
	continueThread();
}

//------------------------------------------------------------------------------
void TSocket::sendToReadyQueue(const TTransferInfo& transferInfo)
{
	mReadyQueue.put(transferInfo);
	mAppSem.release();
	if(mParams.onTransferReady) {
		UDP_LIB::TNetAddr hostAddr = { getWrapper()->getHostAddr(), getWrapper()->getHostPort()};
		UDP_LIB::TNetAddr peerAddr = { mPeerAddr.sin_addr.s_addr, htons(mPeerAddr.sin_port) } ;
		(*mParams.onTransferReady)(hostAddr, peerAddr, getDir());
	}
}

//------------------------------------------------------------------------------
#if defined(UDP_DEBUG_CHECK_PARAMS)
void TSocket::testSocketXmitParams(WSAOVERLAPPED* ovl, WSABUF* buf, unsigned packetId)
{
	if(!ovl) {
		_tprintf(TEXT("[ERROR] [testSocketXmitParams] ovl == NULL for %3d packetId\n"),packetId);
	}
	if(!buf) {
		_tprintf(TEXT("[ERROR] [testSocketXmitParams] buf == NULL for %3d packetId\n"),packetId);
	}
}
#endif


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
TSocketRx::TSocketRx(TSocketWrapper* wrapper, const TCHAR* threadName, const UDP_LIB::TParams& params) : TSocket(wrapper,threadName,params)
{
	//---
	if(mParams.socketBufSize) {
		int optValue  = mParams.socketBufSize;
		int optLength = sizeof(optValue);
		::setsockopt(getWrapper()->getSocket(),SOL_SOCKET,SO_RCVBUF,reinterpret_cast<char*>(&optValue),optLength);
		#if defined(UDP_PRINT_DEBUG_INFO)
			int errCode = ::getsockopt(getWrapper()->getSocket(),SOL_SOCKET,SO_RCVBUF,reinterpret_cast<char*>(&optValue),&optLength);
			errCode = getsockopt(getWrapper()->getSocket(),SOL_SOCKET,SO_RCVBUF,(char*)&optValue,&optLength);
			if(errCode == 0) {
				//_tprintf(TEXT("[INFO] [TSocket] %s, SO_RCVBUF: %8d\n"),threadName,optValue);
			}
		#endif
	}
}

//------------------------------------------------------------------------------
TSocketRx::~TSocketRx()
{
	waitEndOfThread();
}

//------------------------------------------------------------------------------
void TSocketRx::start()
{
	for(unsigned bundleId = 0; bundleId < mParams.numBundles; ++bundleId) {
		//sendToSubmitQueue(TTransferInfo(bundleId,UDP_LIB::Ok));
		submitTransfer(TTransferInfo(bundleId,UDP_LIB::Ok));
	}
	TSocket::start();
}

//------------------------------------------------------------------------------
int TSocketRx::socketTransfer(WSABUF* buf, WSAOVERLAPPED* ovl)
{
	DWORD flags = 0;
	
	return  ::WSARecv(
		             getWrapper()->getSocket(), // socket
				     buf,                       // buffers
				     1,                         // buffer count
				     NULL,                      // bytesRvd
				     &flags,                    // flags
					 //NULL,                      // lpFrom
					 //NULL,                      // lpFromLen
				     ovl,                       // overlapped
				     NULL                       // completion routines
				    );
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
TSocketTx::TSocketTx(TSocketWrapper* wrapper, const TCHAR* threadName, const UDP_LIB::TParams& params) : TSocket(wrapper,threadName,params)
{
	//---
	if(mParams.socketBufSize) {
		int optValue  = mParams.socketBufSize;
		int optLength = sizeof(optValue);
		::setsockopt(getWrapper()->getSocket(),SOL_SOCKET,SO_SNDBUF,reinterpret_cast<char*>(&optValue),optLength);
		#if defined(UDP_PRINT_DEBUG_INFO)
			int errCode = ::getsockopt(getWrapper()->getSocket(),SOL_SOCKET,SO_SNDBUF,reinterpret_cast<char*>(&optValue),&optLength);
			errCode = getsockopt(getWrapper()->getSocket(),SOL_SOCKET,SO_SNDBUF,(char*)&optValue,&optLength);
			if(errCode == 0) {
				//_tprintf(TEXT("[INFO] [TSocket] %s, SO_SNDBUF: %8d\n"),threadName,optValue);
			}
		#endif
	}
}

//------------------------------------------------------------------------------
TSocketTx::~TSocketTx()
{
	waitEndOfThread();
}

//------------------------------------------------------------------------------
void TSocketTx::start()
{
	for(unsigned bundleId = 0; bundleId < mParams.numBundles; ++bundleId) {
		sendToReadyQueue(TTransferInfo(bundleId,UDP_LIB::Ok));
	}
	TSocket::start();
}

//------------------------------------------------------------------------------
int TSocketTx::socketTransfer(WSABUF* buf, WSAOVERLAPPED* ovl)
{
	DWORD flags = 0;
	return ::WSASendTo(
						getWrapper()->getSocket(), // socket
						buf,                       // buffers
						1,                         // buffer count
						NULL,					   // bytesSent
						flags,                     // flags
						getPeer(),                 // destination
						getPeerLen(),              // length of destination params
						ovl,                       // overlapped
						NULL                       // completion routines
					  );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
TSocketWrapper* TSocketWrapper::createSocket(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams* rxParams, const UDP_LIB::TParams* txParams, UDP_LIB::TStatus& status)
{
	status = UDP_LIB::Ok;

	//---
	SOCKET sock = ::WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if(sock == INVALID_SOCKET) {
		#if defined(UDP_PRINT_DEBUG_ERROR)
			_tprintf(TEXT("[ERROR] [WSASocket] %d\n"),::WSAGetLastError());
		#endif
		status = UDP_LIB::SocketCreationError;
		return 0;
	}

	//--
	#if defined(UDP_SO_REUSEADDR)
		BOOL optVal = TRUE;
		int optLen = sizeof(optVal);
		setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optVal, optLen);
		getsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optVal, &optLen);
		_tprintf(TEXT("[DEBUG] SO_REUSEADDR option: %d\n"),optVal);
	#endif
	//--


	//---
	sockaddr_in sockHostAddr;
	sockHostAddr.sin_family      = AF_INET;
	sockHostAddr.sin_addr.s_addr = hostAddr;
	sockHostAddr.sin_port        = htons(hostPort);

	if(bind(sock,(SOCKADDR*)&sockHostAddr,sizeof(sockHostAddr)) != 0) {
		#if defined(UDP_PRINT_DEBUG_ERROR)
			_tprintf(TEXT("[ERROR] [bind] %d\n"),::WSAGetLastError());
		#endif
		status = UDP_LIB::SocketBindError;
		return 0;
	}

	//---
	return new TSocketWrapper(sock,hostAddr,hostPort,rxParams,txParams);
}

//------------------------------------------------------------------------------
TSocketWrapper::TSocketWrapper(SOCKET sock, unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams* rxParams, const UDP_LIB::TParams* txParams) :
                                                                                         mSocket(sock),
	                                                                                     mHostAddr(hostAddr),
																						 mHostPort(hostPort),
																						 mSocketRx(0),
																						 mSocketTx(0)
{
	in_addr inAddr;
	inAddr.S_un.S_addr = mHostAddr;

	#if defined(UDP_PRINT_DEBUG_INFO)
		//printf("[INFO] [TSocketWrapper] HostAddr: %s, Port: %d\n",inet_ntoa(inAddr),hostPort);
	#endif

	const unsigned BufSize = 128;
	TCHAR strBuf[BufSize];
	u_char s_b1 = inAddr.S_un.S_un_b.s_b1;
	u_char s_b2 = inAddr.S_un.S_un_b.s_b2;
	u_char s_b3 = inAddr.S_un.S_un_b.s_b3;
	u_char s_b4 = inAddr.S_un.S_un_b.s_b4;

	#if defined(UDP_PRINT_DEBUG_INFO)
		_stprintf_s(strBuf, BufSize, TEXT("%03d.%03d.%03d.%03d:%05d"), s_b1, s_b2, s_b3, s_b4, mHostPort);
		_tprintf(TEXT("\n[INFO] [TSocketWrapper] HostAddr: %s\n"), strBuf);
	#endif
	if (rxParams) {
		_stprintf_s(strBuf, BufSize, TEXT("%03d.%03d.%03d.%03d:%05d [Rx]"), s_b1, s_b2, s_b3, s_b4, mHostPort);
		mSocketRx = new TSocketRx(this,strBuf,*rxParams);
	}
	if(txParams) {
		_stprintf_s(strBuf, BufSize, TEXT("%03d.%03d.%03d.%03d:%05d [Tx]"), s_b1, s_b2, s_b3, s_b4, mHostPort);
		mSocketTx = new TSocketTx(this,strBuf,*txParams);
	}

	//---
	if(mSocketRx) {
		mSocketRx->start();
	}
	if(mSocketTx) {
		mSocketTx->start();
	}
}

//------------------------------------------------------------------------------
TSocketWrapper::~TSocketWrapper() 
{
	BOOL res = ::CancelIo((HANDLE)mSocket); 
	#if defined(UDP_PRINT_DEBUG_ERROR)
		if(res == FALSE) {
			_tprintf(TEXT("[ERROR] [CancelIo]\n"));
		}
	#endif

	if(closesocket(mSocket) != 0) {
		#if defined(UDP_PRINT_DEBUG_ERROR)
			_tprintf(TEXT("[ERROR] [closesocket] %d\n"),::WSAGetLastError());
		#endif
	}

	delete mSocketRx;
	delete mSocketTx;

	#if defined(UDP_PRINT_DEBUG_INFO)
		_tprintf(TEXT("[INFO] [~TSocketWrapper]\n"));
	#endif
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketWrapper::submitTransfer(UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer)
{
	if(dir == UDP_LIB::Transmit && mSocketTx) {
		return mSocketTx->submitTransfer(transfer);
	}
	if(dir == UDP_LIB::Receive && mSocketRx) {
		return mSocketRx->submitTransfer(transfer);
	}
	return UDP_LIB::SocketXmitNotExist;
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketWrapper::getTransfer(UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer, DWORD timeout)
{
	if(dir == UDP_LIB::Transmit && mSocketTx) {
		return mSocketTx->getTransfer(transfer,timeout);
	}
	if(dir == UDP_LIB::Receive && mSocketRx) {
		return mSocketRx->getTransfer(transfer,timeout);
	}
	return UDP_LIB::SocketXmitNotExist;
}

//------------------------------------------------------------------------------
unsigned TSocketWrapper::getReadyTransferNum(UDP_LIB::TDirection dir) const
{
	if(dir == UDP_LIB::Transmit && mSocketTx) {
		return mSocketTx->getReadyTransferNum();
	}
	if(dir == UDP_LIB::Receive && mSocketRx) {
		return mSocketRx->getReadyTransferNum();
	}
	return 0;
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketWrapper::tryGetTransfer(UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer)
{
	if(dir == UDP_LIB::Transmit && mSocketTx) {
		return mSocketTx->tryGetTransfer(transfer);
	}
	if(dir == UDP_LIB::Receive && mSocketRx) {
		return mSocketRx->tryGetTransfer(transfer);
	}
	return UDP_LIB::SocketXmitNotExist;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
TSocketPool* TSocketPool::mInstance = 0;
#if defined(UDP_THREAD_SAFE)
	TWinCsGuard  TSocketPool::mInstanceGuard;
#endif

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::init()
{
	#if defined(UDP_THREAD_SAFE)
		TWinCsGuard::TLocker lock(mInstanceGuard);
	#endif
	mInstance = new TSocketPool;
	return UDP_LIB::Ok;
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::cleanUp()
{
	#if defined(UDP_THREAD_SAFE)
		TWinCsGuard::TLocker lock(mInstanceGuard);
	#endif
	delete mInstance;
	mInstance = 0;
	return UDP_LIB::Ok;
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::getStatus()
{ 
	#if defined(UDP_THREAD_SAFE)
		TWinCsGuard::TLocker lock(mInstanceGuard);
	#endif
	return getStatusImpl();
}


//------------------------------------------------------------------------------
TSocketPool::TSocketPool() : mNetStatus(UDP_LIB::Ok), mSocketPoolMap()
{
	static const WORD VersionRequested = MAKEWORD(2,2);
	WSADATA wsaData;
	if(::WSAStartup(VersionRequested, &wsaData) != 0) {
		#if defined(UDP_PRINT_DEBUG_ERROR)
			_tprintf(TEXT("[ERROR] [WSAStartup] %d\n"),::WSAGetLastError());
		#endif
		mNetStatus = UDP_LIB::LibInitError;
	}

	#if defined(UDP_PRINT_DEBUG_INFO)
		_tprintf(TEXT("[INFO] [TSocketPool] status: %d\n"),mNetStatus);
		if(mNetStatus == UDP_LIB::Ok) {
			_tprintf(TEXT("[INFO] Version:           0x%04x\n"),wsaData.wVersion);
			_tprintf(TEXT("[INFO] HighVersion:       0x%04x\n"),wsaData.wHighVersion);
			printf("[INFO] Description:       %s\n",wsaData.szDescription);
			printf("[INFO] SystemStatus:      %s\n",wsaData.szSystemStatus);
		}
  	    _tprintf(TEXT("\n"));
	#endif
}

//------------------------------------------------------------------------------
TSocketPool::~TSocketPool()
{
    for(TPoolMap::iterator sock = getPool().begin(); sock != getPool().end(); ++sock) {
		#if defined(UDP_PRINT_DEBUG_INFO)
			in_addr inAddr; inAddr.S_un.S_addr = int32_t(sock->first >> 32);
			TCHAR buf[256];
			InetNtop(AF_INET,&inAddr,buf,sizeof(buf));
			_tprintf(TEXT("[INFO] [cleanPool] Socket deleted. HostAddr: %s, Port: %d\n"),buf,uint32_t(sock->first & 0xFFFFFFFF));
		#endif
        delete sock->second;
    }
	getPool().clear();
	if(::WSACleanup() != 0) {
		//status = UDP_LIB::CleanUpError;
		#if defined(UDP_PRINT_DEBUG_ERROR)
			_tprintf(TEXT("[ERROR] [WSACleanup] %d\n"),::WSAGetLastError());
		#endif
	}

	#if defined(UDP_PRINT_DEBUG_INFO)
		_tprintf(TEXT("\n[INFO] [~TSocketPool]\n"));
	#endif
}

//------------------------------------------------------------------------------
TSocketWrapper* TSocketPool::getSocketWrapper(unsigned long hostAddr, unsigned hostPort)
{
	#if defined(UDP_THREAD_SAFE)
		//TWinCsGuard::TLocker lock(mInstanceGuard);
	#endif

	//---
	if (getStatusImpl() != UDP_LIB::Ok) {
		return 0;
	}

	TPoolMap::iterator sock = getPool().find(getHostAddr64(hostAddr,hostPort));
	if(sock != getPool().end()) {
		return sock->second;
	} else {
		return 0;
	}
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::getStatusImpl()
{
	if (getInstance() == 0)
		return UDP_LIB::NotInitialized;
	else
		return getInstance()->mNetStatus;
}

//------------------------------------------------------------------------------
bool TSocketPool::isSocketExist(unsigned long hostAddr, unsigned hostPort)
{
	#if defined(UDP_THREAD_SAFE)
		TWinCsGuard::TLocker lock(mInstanceGuard);
	#endif

	//---
	if (getStatusImpl() != UDP_LIB::Ok) {
		return false;
	}
	TPoolMap::iterator sock = getPool().find(getHostAddr64(hostAddr,hostPort));
	return (sock != getPool().end());
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::createSocket(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams* rxParams, const UDP_LIB::TParams* txParams)
{
	#if defined(UDP_THREAD_SAFE)
		TWinCsGuard::TLocker lock(mInstanceGuard);
	#endif

	//---
	if(getStatusImpl() != UDP_LIB::Ok) {
		return getStatus();
	}
	if(isSocketExist(hostAddr,hostPort)) {
		return UDP_LIB::SocketAlreadyExist;
	}

	//---
	UDP_LIB::TStatus status; 
	TSocketWrapper* socketWrapper = TSocketWrapper::createSocket(hostAddr,hostPort,rxParams, txParams, status);
	if(!socketWrapper) {
		return status;
	}

	//---
	getPool().insert(std::pair<uint64_t,TSocketWrapper*>(getHostAddr64(hostAddr,hostPort),socketWrapper));
	return UDP_LIB::Ok;
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::submitTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer)
{
	#if defined(UDP_THREAD_SAFE)
		TWinCsGuard::TLocker lock(mInstanceGuard);
	#endif

	if (getStatusImpl() != UDP_LIB::Ok) {
		return getStatus();
	}

	TSocketWrapper* socketWrapper = getSocketWrapper(hostAddr,hostPort);
	if(socketWrapper) {
		return socketWrapper->submitTransfer(dir,transfer);
	} else {
		return UDP_LIB::SocketNotExist;
	}
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::getTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer, DWORD timeout)
{
	#if defined(UDP_THREAD_SAFE)
		TWinCsGuard::TLocker lock(mInstanceGuard);
	#endif

	if (getStatusImpl() != UDP_LIB::Ok) {
		return getStatus();
	}

	TSocketWrapper* socketWrapper = getSocketWrapper(hostAddr,hostPort);
	if(socketWrapper) {
		return socketWrapper->getTransfer(dir,transfer,timeout);
	} else {
		return UDP_LIB::SocketNotExist;
	}
}

//------------------------------------------------------------------------------
unsigned TSocketPool::getReadyTransferNum(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir)
{
	#if defined(UDP_THREAD_SAFE)
		TWinCsGuard::TLocker lock(mInstanceGuard);
	#endif

	if (getStatusImpl() != UDP_LIB::Ok) {
		return 0;
	}
	TSocketWrapper* socketWrapper = getSocketWrapper(hostAddr,hostPort);
	if(socketWrapper) {
		return socketWrapper->getReadyTransferNum(dir);
	} else {
		return 0;
	}
}

//------------------------------------------------------------------------------
UDP_LIB::TStatus TSocketPool::tryGetTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer)
{
	#if defined(UDP_THREAD_SAFE)
		TWinCsGuard::TLocker lock(mInstanceGuard);
	#endif

	if (getStatusImpl() != UDP_LIB::Ok) {
		return getStatus();
	}

	TSocketWrapper* socketWrapper = getSocketWrapper(hostAddr,hostPort);
	if(socketWrapper) {
		return socketWrapper->tryGetTransfer(dir,transfer);
	} else {
		return UDP_LIB::SocketNotExist;
	}
}
