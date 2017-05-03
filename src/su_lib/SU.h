//-------------------------------------------------------------------------------------------------
//	system utilities lib
//	The first design and ideas are by Anton Lukashenko (https://github.com/OnixJet)
//-------------------------------------------------------------------------------------------------

#ifndef SU_H
#define SU_H

#include <windows.h>
#include <string>
#include <tchar.h>

// System Utilities
namespace SU { 

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
typedef std::basic_string<TCHAR> TString;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
template<class T> class TAutoExit
{
	public:
		TAutoExit(T& item) : mItem(item) { mItem.enter(); }
		~TAutoExit() { mItem.exit(); }

	private:
		T& mItem;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TCriticalSection
{
	public:
		TCriticalSection()  { ::InitializeCriticalSection(&mCS); }
		~TCriticalSection() { ::DeleteCriticalSection(&mCS); }
		void enter() { ::EnterCriticalSection(&mCS); }
		void leave() { ::LeaveCriticalSection(&mCS); }
		void exit() { leave(); }


	private:
		TCriticalSection(TCriticalSection& CS) {}
		CRITICAL_SECTION mCS;
};

typedef TAutoExit<TCriticalSection> TAutoExitCS;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TEvent
{
	public: 
		typedef enum {_SIGNALED = WAIT_OBJECT_0, _TIME_OUT = WAIT_TIMEOUT} TResult;

		TEvent(bool manualReset = false) : mEvent(CreateEvent(NULL,manualReset,FALSE,NULL)) {}
		TEvent(bool manualReset, bool initialState) : mEvent(CreateEvent(NULL,manualReset,initialState,NULL)) {}
		~TEvent() { ::CloseHandle(mEvent); }
		void set() { ::SetEvent(mEvent); }
		void reset() { ::ResetEvent(mEvent); }
		TResult wait(DWORD timeout) { return (TResult)WaitForSingleObject(mEvent, timeout); }
		HANDLE getHandle() const { return mEvent; }

	private:
		TEvent(const TEvent& ev){}
		HANDLE	mEvent;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TSemaphore
{
	public:
		TSemaphore(LONG initCount, LONG maxCount, LPCTSTR name = 0) : mSemaphore(NULL), mPrevCount(initCount)
		{
			mSemaphore = ::CreateSemaphore(NULL,initCount,maxCount,name);
			if(::GetLastError() == ERROR_ALREADY_EXISTS) {
				_tprintf(TEXT("[WARN] semaphore %s already exists\n"),name);
			}
			//_tprintf(TEXT("[INFO] [TSemaphore constructor] %s 0x%08x\n"),name,mSemaphore);
		}
		~TSemaphore() 
		{
			::CloseHandle(mSemaphore);
			mSemaphore = NULL;
			//_tprintf(TEXT("[INFO] [TSemaphore destructor]\n"));
		}
		BOOL release(LONG releaseCount = 1)
		{
			return ::ReleaseSemaphore(mSemaphore,releaseCount,&mPrevCount);
		}
		DWORD wait(DWORD timeout = INFINITE) { return ::WaitForSingleObject(mSemaphore, timeout); }
		LONG getPrevCount() const { return mPrevCount; }

	private:
		HANDLE mSemaphore;
		LONG mPrevCount;
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class TElapsedTimer
{
	public:
		TElapsedTimer() 
		{
			mFrequency.QuadPart = 0; 
			mStartTime.QuadPart = 0;
			mStopTime.QuadPart = 0;
			BOOL isSupported = calibrate();
		}
  
		BOOL calibrate() { return ::QueryPerformanceFrequency(&mFrequency); }
		LONGLONG getFrequency() const { return mFrequency.QuadPart; }
		void start() { ::QueryPerformanceCounter(&mStartTime); }
		void stop()  { ::QueryPerformanceCounter(&mStopTime); }
		LONGLONG getStopDelta() const // in us
		{
			LONGLONG llDelta = mStopTime.QuadPart - mStartTime.QuadPart;
			return (1000000*llDelta)/mFrequency.QuadPart;
		}

	private:
		LARGE_INTEGER mFrequency; 
		LARGE_INTEGER mStartTime;
		LARGE_INTEGER mStopTime;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TThread
{
	public:
		TThread(const TCHAR* threadName);
		TThread(const TCHAR* threadName, void* security, unsigned stack_size);
		~TThread();

		const TCHAR* getThreadName() const { return mThreadName; } 
		unsigned getId() const { return mThreadId; }
		bool isResumed() const { return mResume; }
		bool isSuspend() const { return !isResumed(); }

		bool suspendThread();
		bool resumeThread();

		int  getThreadPriority() const;
		bool setThreadPriority(int iPriority);

	protected:
		virtual int exec() = 0;
		HANDLE getHandle() const { return mThreadHandle; }

	private:
		static unsigned const MaxNameLen = 128;
		static unsigned __stdcall proc(void* pVoid);

		TCHAR mThreadName[MaxNameLen];
		bool mResume;
		HANDLE mThreadHandle;
		unsigned mThreadId;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
class TThreadExt: public TThread
{
	public:
		static const unsigned LastCallTimeOut = 200;

		TThreadExt(const TCHAR* threadName);
		TThreadExt(const TCHAR* threadName, void* security, UINT stackSize);
		~TThreadExt();

		void continueThread() { mEventCtrl.set(); }
		void pause(){ mEventCtrl.reset(); }
		void waitEndOfThread(unsigned timeOut = LastCallTimeOut);

	protected:
		virtual int exec();
		virtual void onExec() = 0;
  
		void endThread() { mEnd = true; continueThread(); }
		bool isEndThread() const { return mEnd; }

	private:
		bool mEnd;
		TEvent mEventCtrl;
		TEvent mEventExit;
};

};

#endif	// SU_H