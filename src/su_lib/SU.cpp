//-------------------------------------------------------------------------------------------------
//	system utilities lib
//	The first design and ideas are by Anton Lukashenko (https://github.com/OnixJet)
//-------------------------------------------------------------------------------------------------

#include "SU.h"

namespace SU { 

	#include <string.h>
	#include <process.h>
	#include "SU.h"

	//------------------------------------------------------------------------------
	// TThread
	//------------------------------------------------------------------------------

	//------------------------------------------------------------------------------
	TThread::TThread(const TCHAR* threadName): mResume(false)
	{
		_tcscpy_s(mThreadName,MaxNameLen,threadName);
		mThreadHandle = (HANDLE)_beginthreadex(
			                                      NULL, 
				                                  0, 
					                              proc, 
						                          this, 
							                      CREATE_SUSPENDED, 
								                  &mThreadId
								              );
	}

	//------------------------------------------------------------------------------
	TThread::TThread(const TCHAR* threadName, void* security, unsigned stackSize ): mResume(false)
	{
		_tcscpy_s(mThreadName,MaxNameLen,threadName);
		mThreadHandle = (HANDLE)_beginthreadex(
												security, 
												stackSize, 
												proc, 
												this, 
												CREATE_SUSPENDED, 
												&mThreadId
								              );
	}

	//------------------------------------------------------------------------------
	TThread::~TThread()
	{
		BOOL bFlag_ = CloseHandle(mThreadHandle);
		mThreadHandle = NULL;
	}

	//------------------------------------------------------------------------------
	unsigned __stdcall TThread::proc(void* pVoid)
	{
		TThread* pThread = static_cast<TThread*>(pVoid);
		return pThread->exec();
	}

	//------------------------------------------------------------------------------
	bool TThread::suspendThread()
	{
		mResume  = !(::SuspendThread(getHandle()) >= 0);
		return isResumed();
	}

	//------------------------------------------------------------------------------
	bool TThread::resumeThread()
	{
		mResume = (::ResumeThread(getHandle()) >= 0);
		return isResumed();
	}

	//------------------------------------------------------------------------------
	bool TThread::setThreadPriority(int iPriority)
	{
		return (TRUE == ::SetThreadPriority(getHandle(), iPriority));
	}

	//------------------------------------------------------------------------------
	int TThread::getThreadPriority() const
	{
		return ::GetThreadPriority(getHandle());
	}

	//------------------------------------------------------------------------------
	// TThreadExt
	//------------------------------------------------------------------------------

	//------------------------------------------------------------------------------
	TThreadExt::TThreadExt(const TCHAR* threadName): 
		                                             TThread(threadName),
			                                         mEnd(false),
				                                     mEventCtrl(true),
					                                 mEventExit(true)
	{
		
	}

	//------------------------------------------------------------------------------
	TThreadExt::TThreadExt(const TCHAR* threadName, void* security, UINT stackSize ):
		                                                                   TThread(threadName,security,stackSize),
																		   mEnd(false),
												                           mEventCtrl(true),
											                               mEventExit(true)
	{

	}

	//------------------------------------------------------------------------------
	//------------------------------------------------------------------------------
	TThreadExt::~TThreadExt()
	{
		if(isResumed() && !isEndThread())
			waitEndOfThread();
	} 

	//------------------------------------------------------------------------------
	int TThreadExt::exec()
	{
		while(!isEndThread()) {
			mEventCtrl.wait(INFINITE);
			if(isEndThread()) {
				break;
			}
			onExec();
		}
		mEventExit.set();
		return 0;
	}

	//------------------------------------------------------------------------------
	void TThreadExt::waitEndOfThread(unsigned timeOut)
	{
		if(isEndThread())
			return;
		endThread();
		if(mEventExit.wait(timeOut) == TEvent::_TIME_OUT) {
			suspendThread();
			_tprintf(TEXT("Thread %s timeout %d expired\n"),getThreadName(),timeOut);
		}
	}
}