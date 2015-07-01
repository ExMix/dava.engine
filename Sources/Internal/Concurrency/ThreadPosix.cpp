/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/


#include "Base/Platform.h"
#if !defined(__DAVAENGINE_WINDOWS__)

#include <time.h>
#include <thread>

#include "Concurrency/PosixThreads.h"
#include "Concurrency/Thread.h"

#if defined (__DAVAENGINE_ANDROID__)
#include "Platform/TemplateAndroid/CorePlatformAndroid.h"
#include "Platform/TemplateAndroid/JniHelpers.h"
#endif

#if defined (__DAVAENGINE_APPLE__)
#import <Foundation/NSAutoreleasePool.h>
#endif

namespace DAVA
{
// android have no way to kill a thread, so we will use signal to determine
// if we need to end thread
#if defined (__DAVAENGINE_ANDROID__)
void thread_exit_handler(int sig)
{
	if (SIGRTMIN == sig)
	{
	    JNI::DetachCurrentThreadFromJVM();
		pthread_exit(0);
	}
}
#endif

void Thread::Init()
{
#if defined (__DAVAENGINE_ANDROID__)
    handle = 0;
	struct sigaction cancelThreadAction;
	Memset(&cancelThreadAction, 0, sizeof(cancelThreadAction));
	sigemptyset(&cancelThreadAction.sa_mask);
	cancelThreadAction.sa_flags = 0;
	cancelThreadAction.sa_handler = thread_exit_handler;
	sigaction(SIGRTMIN, &cancelThreadAction, NULL);
#endif
}

void Thread::Shutdown()
{
    DVASSERT(STATE_ENDED == state || STATE_KILLED == state);
    Join();
}

void Thread::KillNative()
{
	uint32 ret = 0;
#if defined (__DAVAENGINE_APPLE__)
    ret = pthread_cancel(handle);
#endif
#if defined (__DAVAENGINE_ANDROID__)
    ret = pthread_kill(handle, SIGRTMIN);
#endif
    if (0 != ret)
    {
        Logger::FrameworkDebug("[Thread::Cancel] cannot kill thread: id = %d, error = %d", Thread::GetCurrentId(), ret);
    }
}

void *PthreadMain(void *param)
{
#if defined(__DAVAENGINE_APPLE__)
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
#endif

#if defined (__DAVAENGINE_ANDROID__)
    JNI::AttachCurrentThreadToJVM();
#endif

#if defined(__DAVAENGINE_DEBUG__) 
    Thread *t = static_cast<Thread *>(param);    
#   if defined (__DAVAENGINE_ANDROID__)
    pthread_setname_np(t->handle, t->name.c_str());
#   elif defined(__DAVAENGINE_APPLE__)
    pthread_setname_np(t->name.c_str());
#   endif
#endif

    Thread::ThreadFunction(param);

#if defined (__DAVAENGINE_ANDROID__)
    JNI::DetachCurrentThreadFromJVM();
#endif

#if defined(__DAVAENGINE_APPLE__)
    [pool release];
#endif
    pthread_exit(0);
}

void Thread::Start()
{
    DVASSERT(STATE_CREATED == state);
    Retain();

    pthread_attr_t attr {};
    pthread_attr_init(&attr);
    if (stack_size != 0)
        pthread_attr_setstacksize(&attr, stack_size);

    pthread_create(&handle, &attr, PthreadMain, (void *)this);
}
    
void Thread::Join()
{
    pthread_join(handle, NULL);
}

Thread::Id Thread::GetCurrentId()
{
    return pthread_self();
}

bool Thread::BindToProcessor(unsigned proc_n)
{
    DVASSERT(proc_n < std::thread::hardware_concurrency());
	if (proc_n >= std::thread::hardware_concurrency())
        return false;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(proc_n, &cpuset);

    int error = pthread_setaffinity_np(handle, sizeof(cpuset), &cpuset);
    return error == 0;
}

} //  namespace DAVA

#endif
