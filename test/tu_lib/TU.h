#ifndef TU_H
#define TU_H

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <string>
#include <tchar.h>
#include <cstdint>

// Test Utilities
namespace TU { 

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
typedef std::basic_string<TCHAR> TString;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
template<typename T> T fillBuf(uint8_t* buf, unsigned bufLen, T firstValue, T incValue = 1)
{
	bufLen = bufLen/sizeof(T);
	T* bufPtr = reinterpret_cast<T*>(buf);
	T value = firstValue;
	while(bufLen--) {
		*bufPtr++ = value;
		value += incValue;
	}
	return value;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
template<typename T> void typeBuf(uint8_t* buf, unsigned bufLen)
{
	bufLen = bufLen/sizeof(T);
	T* bufPtr = reinterpret_cast<T*>(buf);

	_tprintf(TEXT("[0] = %d\n"),bufPtr[0]);
	_tprintf(TEXT("[1] = %d\n"),bufPtr[1]);
	_tprintf(TEXT("[2] = %d\n"),bufPtr[2]);
	_tprintf(TEXT("[3] = %d\n"),bufPtr[3]);

	_tprintf(TEXT("[N-4] = %d\n"),bufPtr[bufLen-4]);
	_tprintf(TEXT("[N-3] = %d\n"),bufPtr[bufLen-3]);
	_tprintf(TEXT("[N-2] = %d\n"),bufPtr[bufLen-2]);
	_tprintf(TEXT("[N-1] = %d\n"),bufPtr[bufLen-1]);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
TCHAR* num2str(uint64_t num);

//------------------------------------------------------------------------------
void packetGen(uint8_t* buf, unsigned bufLen, uint32_t packetNum, bool fillBuf = false, unsigned nPackets = 1);
bool checkPacket(uint8_t* buf, unsigned bufLen, uint32_t& packetNum, bool checkBuf = false);
void setPriority(int processPriority, int threadPriority);

//------------------------------------------------------------------------------
class TStreamTester
{
	public:
		typedef uint32_t TData;

		TStreamTester(TData  increment = 1, TData startValue = 0) : mIncrement(increment), mValue(startValue) {}
		void fillBuf(uint8_t* buf, unsigned len);
		unsigned checkBuf(uint8_t* buf, unsigned len, bool printDiff = false);

	private:
		const TData mIncrement;
		TData	 	mValue; 
};


};

#endif	// TU_H