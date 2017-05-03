//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#if !defined(UDP_H)
#define UDP_H

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <tchar.h>
#include <cstdio>

#include <cstdint>
#include <map>

#include "SU.h"
#include "TQueue.h"
#include "UDP_Defs.h"

#define UDP_SO_REUSEADDR

#define CANCEL_IO_EX_SUPPORTED

#define UDP_PRINT_DEBUG_INFO
#define UDP_PRINT_DEBUG_ERROR
#define UDP_DEBUG_CHECK_PARAMS

#define UDP_THREAD_SAFE

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TSocketWrapper;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TSocketPacket : public WSAOVERLAPPED
{
	public:
		TSocketPacket(unsigned packetId, unsigned bundleId, char* buf, u_long bufLen);
		~TSocketPacket() {}
		WSABUF* getBuf() { return &mBuf; }
		WSAEVENT getEvent() { return mEvent.getHandle(); }
		unsigned getPacketId() const { return mPacketId; }
		unsigned getBundleId() const { return mBundleId; }

	private:
		const unsigned mPacketId;
		const unsigned mBundleId;
		
		WSABUF mBuf;
		SU::TEvent mEvent;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TSocketPacketBundle
{
	public:
		TSocketPacketBundle(unsigned bundleId, unsigned numPacketsInBundle, unsigned netPacketSize);
		~TSocketPacketBundle();
		unsigned getBundleId() const { return mBundleId; }
		WSABUF* getBuf(unsigned nBuf) { return (nBuf < mNumPacketsInBundle) ? mPacketBundle[nBuf]->getBuf() : 0; }
		uint8_t* getBuf() { return mBuf; }
		TSocketPacket* getPacket(unsigned nBuf) { return (nBuf < mNumPacketsInBundle) ? mPacketBundle[nBuf] : 0; }
		const WSAEVENT* getEventArray() { return mEvents; }

	private:
		const unsigned mBundleId;
		const unsigned mNumPacketsInBundle;
		uint8_t* mBuf;
		TSocketPacket** mPacketBundle;
		WSAEVENT* mEvents;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TSocket : public SU::TThreadExt
{
	friend class TSocketWrapper;

	public:
		TSocket(TSocketWrapper* wrapper, const TCHAR* threadName, const UDP_LIB::TParams& params);
		virtual ~TSocket();
		TSocketWrapper* getWrapper() { return mWrapper; }
		virtual UDP_LIB::TDirection getDir() const = 0;

	protected:
		//---
		struct TTransferInfo
		{
			TTransferInfo() : transferId(-1), status(UDP_LIB::NotInitialized), length(0) {}
			TTransferInfo(int id, UDP_LIB::TStatus st = UDP_LIB::NotInitialized, int len = 0) : transferId(id), status(st), length(len) {}

			int transferId;
			UDP_LIB::TStatus status;
			int length;
		};
		struct TTransferStatus
		{
			TTransferStatus() : nCompleted(0), nPending(0), nError(0) {}
			bool setStatus(int status);

			unsigned nCompleted;
			unsigned nPending;
			unsigned nError;
		};

		//---
		UDP_LIB::TParams mParams;
		sockaddr_in mPeerAddr;

		//---
		void onExec();
		const SOCKADDR* getPeer() const { return reinterpret_cast<const SOCKADDR*>(&mPeerAddr); }
		int getPeerLen() const { return sizeof(mPeerAddr); }
		virtual int socketTransfer(WSABUF* buf, WSAOVERLAPPED* ovl) = 0;
		UDP_LIB::TStatus submitTransfer(UDP_LIB::Transfer& transfer);
		UDP_LIB::TStatus submitTransfer(const TTransferInfo& transferInfo);
		UDP_LIB::TStatus getTransfer(UDP_LIB::Transfer& transfer, DWORD timeout = INFINITE);
		UDP_LIB::TStatus getTransfer(TTransferInfo& transferInfo, DWORD timeout = INFINITE);
		unsigned getReadyTransferNum() const { return static_cast<unsigned>(mReadyQueue.size()); }
		UDP_LIB::TStatus tryGetTransfer(UDP_LIB::Transfer& transfer);
		bool clearTransfer(const TTransferInfo& transferInfo);

		virtual void start();
		void sendToReadyQueue(const TTransferInfo& transferInfo);
		void sendToSubmitQueue(const TTransferInfo& transferInfo) {
			mSubmitQueue.put(transferInfo);
			mDrvSem.release();
		}
		WSABUF* getBuf(unsigned nBundle, unsigned nBuf) {  return (nBundle < mParams.numBundles) ? mBundleArray[nBundle]->getBuf(nBuf) : 0; }
		uint8_t* getBuf(unsigned nBundle) { return (nBundle < mParams.numBundles) ? mBundleArray[nBundle]->getBuf() : 0; }
		TSocketPacket* getPacket(unsigned nBundle, unsigned nBuf) { return (nBundle < mParams.numBundles) ? mBundleArray[nBundle]->getPacket(nBuf) : 0;  }
		const WSAEVENT* getEventArray(unsigned nBundle) { return (nBundle < mParams.numBundles) ? mBundleArray[nBundle]->getEventArray() : 0;  }
		int getBufLength() const { return mParams.netPacketSize*mParams.numPacketsInBundle; }
		int isStream() const { return (mParams.numPacketsInBundle != 1); }

		#if defined(UDP_DEBUG_CHECK_PARAMS)
			void testSocketXmitParams(WSAOVERLAPPED* ovl, WSABUF* buf, unsigned packetId);
		#endif

	private:
		typedef TQueue<TTransferInfo,TQueueSl,TWinCsGuard> TJobQueue;

		TSocketWrapper* mWrapper;
		SU::TSemaphore mDrvSem;
		SU::TSemaphore mAppSem;
		TSocketPacketBundle** mBundleArray;
		TJobQueue mSubmitQueue;
		TJobQueue mReadyQueue;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TSocketRx : public TSocket
{
	public:
		TSocketRx(TSocketWrapper* wrapper, const TCHAR* threadName, const UDP_LIB::TParams& params);
		~TSocketRx();
		virtual UDP_LIB::TDirection getDir() const { return UDP_LIB::Receive; }

	protected:
		UDP_LIB::TStatus socketTransfer(TTransferInfo& transferInfo);
		void start();
		virtual int socketTransfer(WSABUF* buf, WSAOVERLAPPED* ovl);
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TSocketTx : public TSocket
{
	public:
		TSocketTx(TSocketWrapper* wrapper, const TCHAR* threadName, const UDP_LIB::TParams& params);
		~TSocketTx();
		virtual UDP_LIB::TDirection getDir() const { return UDP_LIB::Transmit; }

	protected:
		void start();
		virtual int socketTransfer(WSABUF* buf, WSAOVERLAPPED* ovl);
};


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TSocketWrapper
{
	public:
		static TSocketWrapper* createSocket(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams* rxParams, const UDP_LIB::TParams* txParams, UDP_LIB::TStatus& status);
		~TSocketWrapper();
		SOCKET getSocket() { return mSocket; }
		unsigned long getHostAddr() const { return mHostAddr; }
		unsigned getHostPort() const { return mHostPort; }
		UDP_LIB::TStatus submitTransfer(UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer);
		UDP_LIB::TStatus getTransfer(UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer, DWORD timeout = INFINITE);
		unsigned getReadyTransferNum(UDP_LIB::TDirection dir) const;
		UDP_LIB::TStatus tryGetTransfer(UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer);

	private:
		TSocketWrapper(SOCKET sock, unsigned long hostAddr,unsigned hostPort, const UDP_LIB::TParams* rxParams, const UDP_LIB::TParams* txParams);

		SOCKET        mSocket;
		unsigned long mHostAddr;
		unsigned      mHostPort;
		TSocket*      mSocketRx;
		TSocket*      mSocketTx;
};


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TSocketPool
{
	public:
		static UDP_LIB::TStatus init();
		static UDP_LIB::TStatus cleanUp();
		static UDP_LIB::TStatus getStatus(); 
		static bool isSocketExist(unsigned long hostAddr, unsigned hostPort);
		static UDP_LIB::TStatus createSocket(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams* rxParams, const UDP_LIB::TParams* txParams);
		static UDP_LIB::TStatus submitTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer);
		static UDP_LIB::TStatus getTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer, DWORD timeout = INFINITE);
		static unsigned getReadyTransferNum(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir);
		static UDP_LIB::TStatus tryGetTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer);

	private:
		static TSocketPool* mInstance;
		#if defined(UDP_THREAD_SAFE)
			static TWinCsGuard  mInstanceGuard;
		#endif
		typedef std::map<uint64_t, TSocketWrapper*> TPoolMap;

		//---
		static TSocketPool* getInstance() { return mInstance; }
		static TPoolMap& getPool() { return getInstance()->mSocketPoolMap; }
		static uint64_t getHostAddr64(unsigned long hostAddr, unsigned hostPort) { return (static_cast<uint64_t>(hostAddr) << 32) | (hostPort & 0xFFFF); } 
		TSocketPool();
		~TSocketPool();
		static TSocketWrapper* getSocketWrapper(unsigned long hostAddr, unsigned hostPort);
		static UDP_LIB::TStatus getStatusImpl();

		//---
		UDP_LIB::TStatus mNetStatus;
		TPoolMap         mSocketPoolMap;
};


#endif /* UDP_H */

