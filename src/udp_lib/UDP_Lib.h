//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#ifndef UDP_LIB_H
#define UDP_LIB_H

#ifdef __cplusplus
    extern "C" {
#endif

#include "UDP_Defs.h"

#ifdef UDP_LIB_EXPORT
	#define UDP_DLL_API __declspec(dllexport)
	//#define UDP_DLL_API // TODO: this "empty" definition remove warn LNK4197 - fix this problem, (!!!) doesn't work in debug config
#else
    #define UDP_DLL_API __declspec(dllimport)
#endif

namespace UDP_LIB
{
	UDP_DLL_API UDP_LIB::TStatus init();
	UDP_DLL_API UDP_LIB::TStatus cleanUp();
	UDP_DLL_API UDP_LIB::TStatus getStatus();
	UDP_DLL_API bool isSocketExist(unsigned long hostAddr, unsigned hostPort);
	UDP_DLL_API UDP_LIB::TStatus createSocket(unsigned long hostAddr, unsigned hostPort, const UDP_LIB::TParams* rxParams, const UDP_LIB::TParams* txParams);
	UDP_DLL_API UDP_LIB::TStatus submitTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer);
	UDP_DLL_API UDP_LIB::TStatus getTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer, unsigned timeout = INFINITE);
    UDP_DLL_API unsigned getReadyTransferNum(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir);
	UDP_DLL_API UDP_LIB::TStatus tryGetTransfer(unsigned long hostAddr, unsigned hostPort, UDP_LIB::TDirection dir, UDP_LIB::Transfer& transfer);
}    

#ifdef __cplusplus
    }
#endif

#endif /* UDP_LIB_H */

