/* $Id$ */
/** @file
 * VirtualBox COM Event class implementation
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <list>
#include <map>
#include <deque>

#include "EventImpl.h"
#include "AutoCaller.h"
#include "Logging.h"

#include <iprt/semaphore.h>
#include <iprt/critsect.h>
#include <iprt/asm.h>

#include <VBox/com/array.h>

struct VBoxEvent::Data
{
    Data()
        :
        mType(VBoxEventType_Invalid),
        mWaitEvent(NIL_RTSEMEVENT),
        mWaitable(FALSE),
        mProcessed(FALSE)
    {}
    VBoxEventType_T         mType;
    RTSEMEVENT              mWaitEvent;
    BOOL                    mWaitable;
    BOOL                    mProcessed;
    ComPtr<IEventSource>    mSource;
};

HRESULT VBoxEvent::FinalConstruct()
{
    m = new Data;
    return S_OK;
}

void VBoxEvent::FinalRelease()
{
    if (m)
    {
        uninit();
        delete m;
        m = 0;
    }
}


HRESULT VBoxEvent::init(IEventSource *aSource, VBoxEventType_T aType, BOOL aWaitable)
{
    HRESULT rc = S_OK;

    AssertReturn(aSource != NULL, E_INVALIDARG);

    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m->mSource = aSource;
    m->mType = aType;
    m->mWaitable = aWaitable;
    m->mProcessed = !aWaitable;

    do {
        if (aWaitable)
        {
            int vrc = ::RTSemEventCreate (&m->mWaitEvent);

            if (RT_FAILURE(vrc))
            {
                AssertFailed ();
                return setError(E_FAIL,
                                tr("Internal error (%Rrc)"), vrc);
            }
        }
    } while (0);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return rc;
}

void VBoxEvent::uninit()
{
    if (!m)
        return;

    m->mProcessed = TRUE;
    m->mType = VBoxEventType_Invalid;
    m->mSource.setNull();

    if (m->mWaitEvent != NIL_RTSEMEVENT)
    {
        Assert(m->mWaitable);
        ::RTSemEventDestroy(m->mWaitEvent);
        m->mWaitEvent = NIL_RTSEMEVENT;
    }
}

STDMETHODIMP VBoxEvent::COMGETTER(Type)(VBoxEventType_T *aType)
{
    CheckComArgNotNull(aType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // never  changes till event alive, no locking?
    *aType = m->mType;
    return S_OK;
}

STDMETHODIMP VBoxEvent::COMGETTER(Source)(IEventSource* *aSource)
{
    CheckComArgOutPointerValid(aSource);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    m->mSource.queryInterfaceTo(aSource);
    return S_OK;
}

STDMETHODIMP VBoxEvent::COMGETTER(Waitable)(BOOL *aWaitable)
{
    CheckComArgNotNull(aWaitable);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // never  changes till event alive, no locking?
    *aWaitable = m->mWaitable;
    return S_OK;
}


STDMETHODIMP VBoxEvent::SetProcessed()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->mProcessed)
        return S_OK;

    m->mProcessed = TRUE;

    // notify waiters
    ::RTSemEventSignal(m->mWaitEvent);

    return S_OK;
}

STDMETHODIMP VBoxEvent::WaitProcessed(LONG aTimeout, BOOL *aResult)
{
    CheckComArgNotNull(aResult);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        if (m->mProcessed)
        {
            *aResult = TRUE;
            return S_OK;
        }

        if (aTimeout == 0)
        {
            *aResult = m->mProcessed;
            return S_OK;
        }
    }

    int vrc = ::RTSemEventWait(m->mWaitEvent, aTimeout);
    AssertMsg(RT_SUCCESS(vrc) || vrc == VERR_TIMEOUT || vrc == VERR_INTERRUPTED,
              ("RTSemEventWait returned %Rrc\n", vrc));

    if (RT_SUCCESS(vrc))
    {
        AssertMsg(m->mProcessed,
                  ("mProcessed must be set here\n"));
        *aResult = m->mProcessed;
    }
    else
    {
        *aResult = FALSE;
    }

    return S_OK;
}

typedef std::list<Bstr> VetoList;
struct VBoxVetoEvent::Data
{
    Data()
        :
        mVetoed(FALSE)
    {}
    BOOL                    mVetoed;
    VetoList                mVetoList;
};

HRESULT VBoxVetoEvent::FinalConstruct()
{
    VBoxEvent::FinalConstruct();
    m = new Data;
    return S_OK;
}

void VBoxVetoEvent::FinalRelease()
{
    if (m)
    {
        uninit();
        delete m;
        m = 0;
    }
    VBoxEvent::FinalRelease();
}


HRESULT VBoxVetoEvent::init(IEventSource *aSource, VBoxEventType_T aType)
{
    HRESULT rc = S_OK;
    // all veto events are waitable
    rc = VBoxEvent::init(aSource, aType, TRUE);
    if (FAILED(rc)) return rc;

    m->mVetoed = FALSE;
    m->mVetoList.clear();

    return rc;
}

void VBoxVetoEvent::uninit()
{
    VBoxEvent::uninit();
    if (!m)
        return;
    m->mVetoed = FALSE;
}

STDMETHODIMP VBoxVetoEvent::AddVeto(IN_BSTR aVeto)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (aVeto)
        m->mVetoList.push_back(aVeto);

    m->mVetoed = TRUE;

    return S_OK;
}

STDMETHODIMP VBoxVetoEvent::IsVetoed(BOOL * aResult)
{
    CheckComArgOutPointerValid(aResult);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aResult = m->mVetoed;

    return S_OK;
}

STDMETHODIMP  VBoxVetoEvent::GetVetos(ComSafeArrayOut(BSTR, aVetos))
{
    if (ComSafeArrayOutIsNull(aVetos))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    com::SafeArray<BSTR> vetos(m->mVetoList.size());
    int i = 0;
    for (VetoList::const_iterator it = m->mVetoList.begin();
         it != m->mVetoList.end();
         ++it, ++i)
    {
        const Bstr &str = *it;
        str.cloneTo(&vetos[i]);
    }
    vetos.detachTo(ComSafeArrayOutArg(aVetos));

    return S_OK;

}

static const int FirstEvent = (int)VBoxEventType_LastWildcard + 1;
static const int LastEvent  = (int)VBoxEventType_Last;
static const int NumEvents  = LastEvent - FirstEvent;

class ListenerRecord;
typedef std::list<ListenerRecord*> EventMap[NumEvents];
typedef std::map<IEvent*, int32_t> PendingEventsMap;
typedef std::deque<ComPtr<IEvent> > PassiveQueue;

class ListenerRecord
{
private:
    ComPtr<IEventListener>        mListener;
    BOOL                          mActive;
    EventSource*                  mOwner;

    RTSEMEVENT                    mQEvent;
    RTCRITSECT                    mcsQLock;
    PassiveQueue                  mQueue;
    int32_t                       mRefCnt;

public:
    ListenerRecord(IEventListener*                    aListener,
                   com::SafeArray<VBoxEventType_T>&   aInterested,
                   BOOL                               aActive,
                   EventSource*                       aOwner);
    ~ListenerRecord();

    HRESULT process(IEvent* aEvent, BOOL aWaitable, PendingEventsMap::iterator& pit, AutoLockBase& alock);
    HRESULT enqueue(IEvent* aEvent);
    HRESULT dequeue(IEvent* *aEvent, LONG aTimeout, AutoLockBase& aAlock);
    HRESULT eventProcessed(IEvent * aEvent, PendingEventsMap::iterator& pit);
    void addRef()
    {
        ASMAtomicIncS32(&mRefCnt);
    }
    void release()
    {
        if (ASMAtomicDecS32(&mRefCnt) <= 0) delete this;
    }
    BOOL isActive()
    {
        return mActive;
    }

    friend class EventSource;
};

/* Handy class with semantics close to ComPtr, but for ListenerRecord */
class ListenerRecordHolder
{
public:
    ListenerRecordHolder(ListenerRecord* lr)
    :
    held(lr)
    {
        addref();
    }
    ListenerRecordHolder(const ListenerRecordHolder& that)
    :
    held(that.held)
    {
        addref();
    }
    ListenerRecordHolder()
    :
    held(0)
    {
    }
    ~ListenerRecordHolder()
    {
        release();
    }

    ListenerRecord* obj()
    {
        return held;
    }

    ListenerRecordHolder &operator=(const ListenerRecordHolder &that)
    {
        safe_assign(that.held);
        return *this;
    }
private:
    ListenerRecord* held;

    void addref()
    {
        if (held)
            held->addRef();
    }
    void release()
    {
        if (held)
            held->release();
    }
    void safe_assign (ListenerRecord *that_p)
    {
        if (that_p)
            that_p->addRef();
        release();
        held = that_p;
    }
};

typedef std::map<IEventListener*, ListenerRecordHolder>  Listeners;

struct EventSource::Data
{
    Data() {}
    Listeners                     mListeners;
    EventMap                      mEvMap;
    PendingEventsMap              mPendingMap;
};

/**
 * This function defines what wildcard expands to.
 */
static BOOL implies(VBoxEventType_T who, VBoxEventType_T what)
{
    switch (who)
    {
        case VBoxEventType_Any:
            return TRUE;
        case VBoxEventType_MachineEvent:
            return     (what == VBoxEventType_OnMachineStateChange)
                    || (what == VBoxEventType_OnMachineDataChange)
                    || (what == VBoxEventType_OnMachineRegistered)
                    || (what == VBoxEventType_OnSessionStateChange)
                    || (what == VBoxEventType_OnGuestPropertyChange);
        case VBoxEventType_SnapshotEvent:
            return     (what == VBoxEventType_OnSnapshotTaken)
                    || (what == VBoxEventType_OnSnapshotDeleted)
                    || (what == VBoxEventType_OnSnapshotChange)
                    ;
        case VBoxEventType_Invalid:
            return FALSE;
    }
    return who == what;
}

ListenerRecord::ListenerRecord(IEventListener*                  aListener,
                               com::SafeArray<VBoxEventType_T>& aInterested,
                               BOOL                             aActive,
                               EventSource*                     aOwner)
    :
    mActive(aActive),
    mOwner(aOwner),
    mRefCnt(0)
{
    mListener = aListener;
    EventMap* aEvMap = &aOwner->m->mEvMap;

    for (size_t i = 0; i < aInterested.size(); ++i)
    {
        VBoxEventType_T interested = aInterested[i];
        for (int j = FirstEvent; j < LastEvent; j++)
        {
            VBoxEventType_T candidate = (VBoxEventType_T)j;
            if (implies(interested, candidate))
            {
                (*aEvMap)[j - FirstEvent].push_back(this);
            }
        }
    }

    if (!mActive)
    {
        ::RTCritSectInit(&mcsQLock);
        ::RTSemEventCreate (&mQEvent);
    }
}

ListenerRecord::~ListenerRecord()
{
    /* Remove references to us from the event map */
    EventMap* aEvMap = &mOwner->m->mEvMap;
    for (int j = FirstEvent; j < LastEvent; j++)
    {
        (*aEvMap)[j - FirstEvent].remove(this);
    }

    if (!mActive)
    {
        ::RTCritSectDelete(&mcsQLock);
        ::RTSemEventDestroy(mQEvent);
    }
}

HRESULT ListenerRecord::process(IEvent*                     aEvent,
                                BOOL                        aWaitable,
                                PendingEventsMap::iterator& pit,
                                AutoLockBase&               aAlock)
{
    if (mActive)
    {
        /*
         * We release lock here to allow modifying ops on EventSource inside callback.
         */
        HRESULT rc =  S_OK;
        if (mListener)
        {
            aAlock.release();
            rc =  mListener->HandleEvent(aEvent);
            aAlock.acquire();
        }
        if (aWaitable)
            eventProcessed(aEvent, pit);
        return rc;
    }
    else
        return enqueue(aEvent);
}


HRESULT ListenerRecord::enqueue (IEvent* aEvent)
{
    AssertMsg(!mActive, ("must be passive\n"));
    // put an event the queue
    ::RTCritSectEnter(&mcsQLock);
    mQueue.push_back(aEvent);
    ::RTCritSectLeave(&mcsQLock);

     // notify waiters
    ::RTSemEventSignal(mQEvent);

    return S_OK;
}

HRESULT ListenerRecord::dequeue (IEvent*       *aEvent,
                                 LONG          aTimeout,
                                 AutoLockBase& aAlock)
{
    AssertMsg(!mActive, ("must be passive\n"));

    ::RTCritSectEnter(&mcsQLock);
    if (mQueue.empty())
    {
        // retain listener record
        ListenerRecordHolder holder(this);
        ::RTCritSectLeave(&mcsQLock);
        // Speed up common case
        if (aTimeout == 0)
        {
            *aEvent = NULL;
            return S_OK;
        }
        // release lock while waiting, listener will not go away due to above holder
        aAlock.release();
        ::RTSemEventWait(mQEvent, aTimeout);
        // reacquire lock
        aAlock.acquire();
        ::RTCritSectEnter(&mcsQLock);
    }
    if (mQueue.empty())
    {
        *aEvent = NULL;
    }
    else
    {
        mQueue.front().queryInterfaceTo(aEvent);
        mQueue.pop_front();
    }
    ::RTCritSectLeave(&mcsQLock);
    return S_OK;
}

HRESULT ListenerRecord::eventProcessed (IEvent* aEvent, PendingEventsMap::iterator& pit)
{
    if (--pit->second == 0)
    {
        Assert(pit->first == aEvent);
        aEvent->SetProcessed();
        mOwner->m->mPendingMap.erase(pit);
    }

    Assert(pit->second >= 0);
    return S_OK;
}

EventSource::EventSource()
{}

EventSource::~EventSource()
{}

HRESULT EventSource::FinalConstruct()
{
    m = new Data;
    return S_OK;
}

void EventSource::FinalRelease()
{
    uninit();
    delete m;
}

HRESULT EventSource::init(IUnknown *)
{
    HRESULT rc = S_OK;

    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();
    return rc;
}

void EventSource::uninit()
{
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
    m->mListeners.clear();
    // m->mEvMap shall be cleared at this point too by destructors, assert?
}

STDMETHODIMP EventSource::RegisterListener(IEventListener * aListener,
                                           ComSafeArrayIn(VBoxEventType_T, aInterested),
                                           BOOL             aActive)
{
    CheckComArgNotNull(aListener);
    CheckComArgSafeArrayNotNull(aInterested);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    Listeners::const_iterator it = m->mListeners.find(aListener);
    if (it != m->mListeners.end())
        return setError(E_INVALIDARG,
                        tr("This listener already registered"));

    com::SafeArray<VBoxEventType_T> interested(ComSafeArrayInArg (aInterested));
    ListenerRecordHolder lrh(new ListenerRecord(aListener, interested, aActive, this));
    m->mListeners.insert(Listeners::value_type(aListener, lrh));

    return S_OK;
}

STDMETHODIMP EventSource::UnregisterListener(IEventListener * aListener)
{
    CheckComArgNotNull(aListener);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    Listeners::iterator it = m->mListeners.find(aListener);
    HRESULT rc;

    if (it != m->mListeners.end())
    {
        m->mListeners.erase(it);
        // destructor removes refs from the event map
        rc = S_OK;
    }
    else
    {
        rc = setError(VBOX_E_OBJECT_NOT_FOUND,
                      tr("Listener was never registered"));
    }

    return rc;
}

STDMETHODIMP EventSource::FireEvent(IEvent * aEvent,
                                    LONG     aTimeout,
                                    BOOL     *aProcessed)
{
    CheckComArgNotNull(aEvent);
    CheckComArgOutPointerValid(aProcessed);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT hrc;
    BOOL aWaitable = FALSE;
    aEvent->COMGETTER(Waitable)(&aWaitable);

    do {
        /* See comment in EventSource::GetEvent() */
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        VBoxEventType_T evType;
        hrc = aEvent->COMGETTER(Type)(&evType);
        AssertComRCReturn(hrc, VERR_ACCESS_DENIED);

        std::list<ListenerRecord*>& listeners = m->mEvMap[(int)evType-FirstEvent];

        /* Anyone interested in this event? */
        uint32_t cListeners = listeners.size();
        if (cListeners == 0)
        {
            aEvent->SetProcessed();
            break; // just leave the lock and update event object state
        }

        PendingEventsMap::iterator pit;

        if (aWaitable)
        {
            m->mPendingMap.insert(PendingEventsMap::value_type(aEvent, cListeners));
            // we keep iterator here to allow processing active listeners without
            // pending events lookup
            pit = m->mPendingMap.find(aEvent);
        }
        for(std::list<ListenerRecord*>::const_iterator it = listeners.begin();
            it != listeners.end(); ++it)
        {
            HRESULT cbRc;
            // keep listener record reference, in case someone will remove it while in callback
            ListenerRecordHolder record(*it);

            /**
             * We pass lock here to allow modifying ops on EventSource inside callback
             * in active mode. Note that we expect list iterator stability as 'alock'
             * could be temporary released when calling event handler.
             */
            cbRc = record.obj()->process(aEvent, aWaitable, pit, alock);

            if (FAILED_DEAD_INTERFACE(cbRc))
            {
                AutoWriteLock awlock(this COMMA_LOCKVAL_SRC_POS);
                Listeners::iterator lit = m->mListeners.find(record.obj()->mListener);
                if (lit != m->mListeners.end())
                    m->mListeners.erase(lit);
            }
            // anything else to do with cbRc?
        }
    } while (0);
    /* We leave the lock here */

    if (aWaitable)
        hrc = aEvent->WaitProcessed(aTimeout, aProcessed);
    else
        *aProcessed = TRUE;

    return hrc;
}


STDMETHODIMP EventSource::GetEvent(IEventListener * aListener,
                                   LONG             aTimeout,
                                   IEvent  **       aEvent)
{

    CheckComArgNotNull(aListener);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /**
     * There's subtle dependency between this lock and one in FireEvent():
     * we need to be able to access event queue in FireEvent() while waiting
     * here, to make this wait preemptible, thus both take read lock (write
     * lock in FireEvent() would do too, and probably is a bit stricter),
     * but will be unable to .
     */
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Listeners::iterator it = m->mListeners.find(aListener);
    HRESULT rc;

    if (it != m->mListeners.end())
        rc = it->second.obj()->dequeue(aEvent, aTimeout, alock);
    else
        rc = setError(VBOX_E_OBJECT_NOT_FOUND,
                      tr("Listener was never registered"));

    return rc;
}

STDMETHODIMP EventSource::EventProcessed(IEventListener * aListener,
                                         IEvent *         aEvent)
{
    CheckComArgNotNull(aListener);
    CheckComArgNotNull(aEvent);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Listeners::iterator it = m->mListeners.find(aListener);
    HRESULT rc;

    BOOL aWaitable = FALSE;
    aEvent->COMGETTER(Waitable)(&aWaitable);

    if (it != m->mListeners.end())
    {
        ListenerRecord* aRecord = it->second.obj();

        if (aRecord->isActive())
            return setError(E_INVALIDARG,
                        tr("Only applicable to passive listeners"));

        if (aWaitable)
        {
            PendingEventsMap::iterator pit = m->mPendingMap.find(aEvent);

            if (pit == m->mPendingMap.end())
            {
                AssertFailed();
                rc = setError(VBOX_E_OBJECT_NOT_FOUND,
                              tr("Unknown event"));
            }
            else
                rc = aRecord->eventProcessed(aEvent, pit);
        }
        else
        {
            // for non-waitable events we're done
            rc = S_OK;
        }
    }
    else
    {
        rc = setError(VBOX_E_OBJECT_NOT_FOUND,
                      tr("Listener was never registered"));
    }

    return rc;
}

/**
 * This class serves as feasible listener implementation
 * which could be used by clients not able to create local
 * COM objects, but still willing to recieve event
 * notifications in passive mode, such as webservices.
 */
class ATL_NO_VTABLE PassiveEventListener :
    public VirtualBoxBase,
    public VirtualBoxSupportErrorInfoImpl<PassiveEventListener, IEventListener>,
    public VirtualBoxSupportTranslation<PassiveEventListener>,
    VBOX_SCRIPTABLE_IMPL(IEventListener)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(PassiveEventListener)

    DECLARE_NOT_AGGREGATABLE(PassiveEventListener)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(PassiveEventListener)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IEventListener)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    PassiveEventListener()
    {}
    ~PassiveEventListener()
    {}

    HRESULT FinalConstruct()
    {
        return S_OK;
    }
    void FinalRelease()
    {}

    // IEventListener methods
    STDMETHOD(HandleEvent)(IEvent *)
    {
        ComAssertMsgRet(false, ("HandleEvent() of wrapper shall never be called"),
                        E_FAIL);
    }
    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"PassiveEventListener"; }
};

#ifdef VBOX_WITH_XPCOM
NS_DECL_CLASSINFO(PassiveEventListener)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(PassiveEventListener, IEventListener)
NS_DECL_CLASSINFO(VBoxEvent)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(VBoxEvent, IEvent)
NS_DECL_CLASSINFO(VBoxVetoEvent)
NS_IMPL_ISUPPORTS_INHERITED1(VBoxVetoEvent, VBoxEvent, IVetoEvent)
NS_DECL_CLASSINFO(EventSource)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(EventSource, IEventSource)
#endif

STDMETHODIMP EventSource::CreateListener(IEventListener ** aListener)
{
    CheckComArgOutPointerValid(aListener);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComObjPtr<PassiveEventListener> listener;

    HRESULT rc = listener.createObject();
    ComAssertMsgRet(SUCCEEDED(rc), ("Could not create wrapper object (%Rrc)", rc),
                    E_FAIL);
    listener.queryInterfaceTo(aListener);
    return S_OK;
}
