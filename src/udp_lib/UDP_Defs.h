//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#if !defined(UDP_DEFS_H)
#define UDP_DEFS_H

#ifdef __cplusplus
	#include <cstdint>
#else
	#include <stdint.h>
#endif

namespace UDP_LIB
{
	typedef enum
	{
		NotInit  = -1,
		Transmit =  0,   // send UDP packets from host
		Receive  =  1    // receive UDP packets to host
	} TDirection;

	typedef enum 
	{
		NoReadyTransfers    =  1,  // not error, info - there are no transfers in ready queue
		Ok                  =  0,  // all ok
		NotInitialized      = -1,  // UDP_LIB not initialized
		LibInitError        = -2,  // UDP_LIB initialization error 
		SocketNotExist      = -3,  // 
		SocketAlreadyExist  = -4,  //
		SocketCreationError = -5,  //
		SocketBindError     = -6,  //
		LibCleanUpError     = -7,  // UDP_LIB cleanup error (deinitialization)
		SubmitError         = -8, 
		SocketTransferError = -9,  // error in TSocket::getTransfer
		SocketWaitTimeout   = -10,
		SocketXmitNotExist  = -11, // Rx or Tx part (channel) not exist in opened socket
		WSAWaitError        = -12, // error in TSocket::onExec()
		WSAOvlError         = -13, // error in TSocket::onExec()
		XmitLenError        = -14  // bytes number really transitted are not equal bytes number set for transmit (error in TSocket::onExec())
	} TStatus;

	struct Transfer
	{
		int           bundleId;    // internal (UDP_LIB related) bundle (of 'atomic' UDP packets) id (number)
		TDirection    direction;   // rx or tx (and "NotInit" esle)
		TStatus       status;      // error status
		int           length;      // actual length of data in packet bundle ('big buffer')
		int           bufLength;   // length of bundle, i.e. length must be always <= bufLength
		uint8_t*      buf;         // pointer to bundle of packets
		int           isStream;    // stream or common transfer, when bundle has only 1 packet transfer is not stream
	};

	struct TNetAddr
	{
		unsigned long ipAddr;
		unsigned      port;
	};

	typedef void (*OnTransferReadyFunc)(const TNetAddr& host, const TNetAddr& peer, UDP_LIB::TDirection dir);
	struct TParams
	{
		unsigned            netPacketSize;			// size of 'atomic' UDP packet ('small buffer')
		unsigned            numPacketsInBundle;     // number of 'atomic' UDP packets in bundle
		unsigned            numBundles;				// number of bundles ('big buffers')
		int                 threadPriority;			// priority of internal (UDP_LIB) processing thread 
		unsigned            timeout;                // internal timeot, see TSocket::onExec()
		int                 socketBufSize;			// SO_RCVBUF/SO_SNDBUF parameter (see MSDN)  
		unsigned long       peerAddr;				// peer (another side) IP address
		unsigned            peerPort;				// peer (another side) IP port
		OnTransferReadyFunc onTransferReady;        // callback function (notifier), called from TSocket::onExec() at the end of (!) 'sendToReadyQueue'
	};
}    

#endif /* UDP_DEFS_H */

