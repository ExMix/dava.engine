#ifndef __DAVA_SIGNAL_H__
#define __DAVA_SIGNAL_H__

#include "Base/BaseTypes.h"
#include "Concurrency/Mutex.h"
#include "Concurrency/LockGuard.h"
#include "Concurrency/Thread.h"
#include "Concurrency/Atomic.h"
#include "Functional/Function.h"
#include "Functional/SignalBase.h"

// #define ENABLE_MULTITHREADED_SIGNALS // <-- this still isn't implemented

namespace DAVA
{
namespace Sig11
{
struct DummyMutex
{
    void Lock()
    {
    }
    void Unlock()
    {
    }
    bool TryLock()
    {
        return true;
    }
};

struct DymmyThreadID
{
};

template <typename MutexType, typename ThreadIDType, typename... Args>
class SignalImpl : public SignalBase
{
public:
    using Func = Function<void(Args...)>;

    SignalImpl() = default;
    SignalImpl(const SignalImpl&) = delete;
    SignalImpl& operator=(const SignalImpl&) = delete;

    ~SignalImpl()
    {
        DisconnectAll();
    }

    template <typename Fn>
    SigConnectionID Connect(const Fn& fn, ThreadIDType tid = {})
    {
        LockGuard<MutexType> guard(mutex);
        return AddConnection(nullptr, Func(fn), tid);
    }

    template <typename Obj, typename Cls>
    SigConnectionID Connect(Obj* obj, void (Cls::*const& fn)(Args...), ThreadIDType tid = ThreadIDType())
    {
        LockGuard<MutexType> guard(mutex);
        return AddConnection(TrackedObject::Cast(obj), Func(obj, fn), tid);
    }

    template <typename Obj, typename Cls>
    SigConnectionID Connect(Obj* obj, void (Cls::*const& fn)(Args...) const, ThreadIDType tid = ThreadIDType())
    {
        LockGuard<MutexType> guard(mutex);
        return AddConnection(TrackedObject::Cast(obj), Func(obj, fn), tid);
    }

    void Disconnect(SigConnectionID id)
    {
        LockGuard<MutexType> guard(mutex);

        auto it = connections.find(id);
        if (it != connections.end())
        {
            TrackedObject* obj = it->second.obj;
            if (nullptr != obj)
            {
                obj->Untrack(this);
            }

            connections.erase(it);
        }
    }

    void Disconnect(TrackedObject* obj) override final
    {
        if (nullptr != obj)
        {
            LockGuard<MutexType> guard(mutex);

            auto it = connections.begin();
            auto end = connections.end();

            while (it != end)
            {
                if (it->second.obj == obj)
                {
                    obj->Untrack(this);
                    it = connections.erase(it);
                }
                else
                {
                    it++;
                }
            }
        }
    }

    void DisconnectAll()
    {
        LockGuard<MutexType> guard(mutex);

        for (auto&& con : connections)
        {
            TrackedObject* obj = con.second.obj;
            if (nullptr != obj)
            {
                obj->Untrack(this);
            }
        }

        connections.clear();
    }

    void Track(SigConnectionID id, TrackedObject* obj)
    {
        LockGuard<MutexType> guard(mutex);

        auto it = connections.find(id);
        if (it != connections.end())
        {
            if (nullptr != it->second.obj)
            {
                it->second.obj->Untrack(this);
                it->second.obj = nullptr;
            }

            if (nullptr != obj)
            {
                it->second.obj = obj;
                obj->Track(this);
            }
        }
    }

    TrackedObject* GetTracked(SigConnectionID id) const
    {
        TrackedObject* ret = nullptr;

        auto it = connections.find(id);
        if (it != connections.end())
        {
            ret = it->second.obj;
        }

        return ret;
    }

    void Block(SigConnectionID id, bool block)
    {
        auto it = connections.find(id);
        if (it != connections.end())
        {
            it->second.blocked = block;
        }
    }

    bool IsBlocked(SigConnectionID id) const
    {
        bool ret = false;

        auto it = connections.find(id);
        if (it != connections.end())
        {
            ret = it->second.blocked;
        }

        return ret;
    }

    virtual void Emit(Args...) = 0;

protected:
    struct ConnData
    {
        ConnData(Func&& fn_, TrackedObject* obj_, ThreadIDType tid_)
            : fn(std::move(fn_))
            , obj(obj_)
            , tid(tid_)
            , blocked(false)
        {
        }

        Func fn;
        TrackedObject* obj;
        ThreadIDType tid;
        bool blocked;
    };

    MutexType mutex;
    Map<SigConnectionID, ConnData> connections;

private:
    SigConnectionID AddConnection(TrackedObject* obj, Func&& fn, const ThreadIDType& tid)
    {
        SigConnectionID id = SignalBase::GetUniqueConnectionID();
        connections.emplace(std::make_pair(id, ConnData(std::move(fn), obj, tid)));

        if (nullptr != obj)
        {
            obj->Track(this);
        }

        return id;
    }
};

} // namespace Sig11

template <typename... Args>
class Signal final : public Sig11::SignalImpl<Sig11::DummyMutex, Sig11::DymmyThreadID, Args...>
{
public:
    using Base = Sig11::SignalImpl<Sig11::DummyMutex, Sig11::DymmyThreadID, Args...>;

    Signal() = default;
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;

    void Emit(Args... args) override
    {
        auto iter = Base::connections.begin();
        while (iter != Base::connections.end())
        {
            auto next = iter;
            ++next;

            if (!iter->second.blocked)
            {
                // Make functor copy and call its copy:
                //  when connected lambda with captured variables disconnects from signal while signal is emitting
                //  compiler destroys lambda and its captured variables
                auto fn = iter->second.fn;
                fn(args...);
            }

            iter = next;
        }
    }
};

#ifdef ENABLE_MULTITHREADED_SIGNALS

template <typename... Args>
class SignalMt final : public Sig11::SignalImpl<Mutex, Thread::Id, Args...>
{
    using Base = Sig11::SignalImpl<Mutex, Thread::Id, Args...>;

    SignalMt() = default;
    SignalMt(const SignalMt&) = delete;
    SignalMt& operator=(const SignalMt&) = delete;

    void Emit(Args... args) override
    {
        Thread::Id thisTid = Thread::GetCurrentId();

        LockGuard<Mutex> guard(Base::mutex);
        for (auto&& con : Base::connections)
        {
            if (!con.second.blocked)
            {
                if (con.second.tid == thisTid)
                {
                    con.second.fn(args...);
                }
                else
                {
                    Function<void()> fn = Bind(con.second.fn, args...);

                    // TODO:
                    // add implementation
                    // new to send fn variable directly into thread with given id = con.second.tid
                    // ...
                }
            }
        }
    }
};

#endif

} // namespace DAVA

#endif // __DAVA_SIGNAL_H__
