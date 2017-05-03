#include "TU.h"

namespace TU {

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void packetGen(uint8_t* buf, unsigned bufLen, uint32_t bufNum, bool fillBuf, unsigned nPackets)
{
    unsigned packetSize = bufLen/nPackets;

    //printf("bufLen = %d, packetSize = %d, nPackets = %d\n",bufLen,packetSize,nPackets);

    for(unsigned pkt = 0; pkt < nPackets; ++pkt) {
        uint32_t* headerPtr = reinterpret_cast<uint32_t*>(buf);
        *headerPtr = bufNum*nPackets + pkt;

        if(fillBuf) {
            unsigned offset = sizeof(bufNum); 
            for(unsigned i = offset; i < packetSize; ++i) {
                buf[i] = static_cast<uint8_t>(i);
            }
        }
        buf = buf + packetSize;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
bool checkPacket(uint8_t* buf, unsigned bufLen, uint32_t& packetNum, bool checkBuf)
{
    bool checkOk = true;
    uint32_t* headerPtr = reinterpret_cast<uint32_t*>(buf);
    if(*headerPtr != packetNum) {
        packetNum = *headerPtr;
        //checkOk = false;
    }

    if(checkBuf) {
        unsigned offset = sizeof(packetNum); 
        for(unsigned i = offset; i < bufLen; ++i) {
            if(buf[i] != static_cast<uint8_t>(i)) {
                checkOk = false;
                break;
            }
        }
    }
    return checkOk;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
TCHAR* num2str(uint64_t num)
{
    const unsigned BufSize  = 128;
    const unsigned GroupLen = 3;
    const TCHAR GroupDivider = TEXT(',');
    static TCHAR outBuf[BufSize];
    TCHAR tmpBuf[BufSize];

    _sntprintf_s(outBuf,BufSize/2,TEXT("%lld"),num);
    size_t len = _tcslen(outBuf);
    unsigned idxOut = 0;
    unsigned numDigit = 0;
    for(size_t idxIn = len; idxIn; --idxIn) {
        tmpBuf[idxOut++] = outBuf[idxIn-1];
        if((++numDigit == GroupLen) && (idxIn-1)) {
            numDigit = 0;
            tmpBuf[idxOut++] = GroupDivider;
        } 
    }
    tmpBuf[idxOut] = 0;
    len = _tcslen(tmpBuf);
    idxOut = 0;
    for(size_t idxIn = len; idxIn; --idxIn) {
        outBuf[idxOut++] = tmpBuf[idxIn-1];
    }
    outBuf[idxOut] = 0;
    return outBuf;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void setPriority(int processPriority, int threadPriority)
{
    HANDLE hProcess = ::GetCurrentProcess();
    HANDLE hThread  = ::GetCurrentThread();
    ::SetPriorityClass(hProcess,processPriority);
    ::SetThreadPriority(hThread,threadPriority);
    processPriority = ::GetPriorityClass(hProcess);
    threadPriority    = ::GetThreadPriority(hThread);
    _tprintf(TEXT("\n[INFO] process priority: 0x%04x, thread priority: %2d\n\n"),processPriority,threadPriority);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
void TStreamTester::fillBuf(uint8_t* buf, unsigned len)
{
    len /= sizeof(TData);
    TData* dataBuf = reinterpret_cast<TData*>(buf);
    for(unsigned i = 0; i < len; ++i) {
        *dataBuf++ = mValue;
        mValue += mIncrement;
    }
}

//------------------------------------------------------------------------------
unsigned TStreamTester::checkBuf(uint8_t* buf, unsigned len, bool printDiff)
{
    len /= sizeof(TData);
    TData* dataBuf = reinterpret_cast<TData*>(buf);
    unsigned errNum = 0;
    for(unsigned i = 0; i < len; ++i,++dataBuf) {
        if(*dataBuf != mValue) {
            ++errNum;
			if (printDiff) {
				TData delta = (TData)(abs((int64_t)(mValue)-(int64_t)(*dataBuf)));
				_tprintf(TEXT("[DATA CHECK ERROR] expected: %10u, received: %10u, delta: %10u, errNum: %5d\n"), mValue, *dataBuf, delta, errNum);
			}
            mValue = *dataBuf;
        }
        mValue += mIncrement;
    }
    return errNum;
}


}