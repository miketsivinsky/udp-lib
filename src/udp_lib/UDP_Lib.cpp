//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#include "UDP.h"

#define UDP_LIB_EXPORT
#include "UDP_Lib.h"

namespace UDP_LIB
{
	//------------------------------------------------------------------------------
	UDP_LIB::TStatus init()
	{
		return TSocketPool::init();
	}

	//------------------------------------------------------------------------------
	UDP_LIB::TStatus cleanUp()
	{
		return TSocketPool::cleanUp();
	}

	//------------------------------------------------------------------------------
	UDP_LIB::TStatus getStatus()
	{
		return TSocketPool::getStatus();
	}

	//------------------------------------------------------------------------------
	bool isSocketExist(unsigned long hostAddr, unsigned hostPort)
	{
		return TSocketPool::isSocketExist(hostAddr,hostPort);
	}

	//------------------------------------------------------------------------------
	 UDP_LIB::TStatus createSocket(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams* rxParams, const UDP_LIB::TParams* txParams)
	{
		return TSocketPool::createSocket(hostAddr,hostPort,rxParams,txParams);
	}

	//------------------------------------------------------------------------------
	 UDP_LIB::TStatus submitTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer)
	 {
		 return TSocketPool::submitTransfer(hostAddr,hostPort,dir,transfer);
	 }

	//------------------------------------------------------------------------------
	UDP_LIB::TStatus getTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer, unsigned timeout)
	{
		return TSocketPool::getTransfer(hostAddr,hostPort,dir,transfer,static_cast<DWORD>(timeout));
	}

	//------------------------------------------------------------------------------
    UDP_DLL_API unsigned getReadyTransferNum(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir)
	{
		return TSocketPool::getReadyTransferNum(hostAddr,hostPort,dir);
	}
	
	//------------------------------------------------------------------------------
	UDP_DLL_API UDP_LIB::TStatus tryGetTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer)
	{
		return TSocketPool::tryGetTransfer(hostAddr,hostPort,dir,transfer);
	}

}