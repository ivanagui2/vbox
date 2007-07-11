/** @file
 * VirtualBox USB Proxy Service (base) class.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include "USBProxyService.h"
#include "Logging.h"

#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/semaphore.h>



/** @todo add the required locking. */

/**
 * Initialize data members.
 */
USBProxyService::USBProxyService (Host *aHost)
    : mHost (aHost), mThread (NIL_RTTHREAD), mTerminate (false), mDevices (), mLastError (VINF_SUCCESS)
{
    LogFlowThisFunc (("aHost=%p\n", aHost));
}


/**
 * Empty destructor.
 */
USBProxyService::~USBProxyService()
{
    LogFlowThisFunc (("\n"));
    Assert (mThread == NIL_RTTHREAD);
    mDevices.clear();
    mTerminate = true;
    mHost = NULL;
}


bool USBProxyService::isActive (void)
{
    return mThread != NIL_RTTHREAD;
}


int USBProxyService::getLastError (void)
{
    return mLastError;
}


int USBProxyService::start (void)
{
    int rc = VINF_SUCCESS;
    if (mThread == NIL_RTTHREAD)
    {
        /*
         * Force update before starting the poller thread.
         */
        wait (0);
        processChanges ();

        /*
         * Create the poller thread which will look for changes.
         */
        mTerminate = false;
        rc = RTThreadCreate (&mThread, USBProxyService::serviceThread, this,
                             0, RTTHREADTYPE_INFREQUENT_POLLER, RTTHREADFLAGS_WAITABLE, "USBPROXY");
        AssertRC (rc);
        if (VBOX_SUCCESS (rc))
            LogFlowThisFunc (("started mThread=%RTthrd\n", mThread));
        else
        {
            mThread = NIL_RTTHREAD;
            mLastError = rc;
        }
    }
    else
        LogFlowThisFunc (("already running, mThread=%RTthrd\n", mThread));
    return rc;
}


int USBProxyService::stop (void)
{
    int rc = VINF_SUCCESS;
    if (mThread != NIL_RTTHREAD)
    {
        /*
         * Mark the thread for termination and kick it.
         */
        ASMAtomicXchgSize (&mTerminate, true);
        rc = interruptWait();
        AssertRC (rc);

        /*
         * Wait for the thread to finish and then update the state.
         */
        rc = RTThreadWait (mThread, 60000, NULL);
        if (rc == VERR_INVALID_HANDLE)
            rc = VINF_SUCCESS;
        if (VBOX_SUCCESS (rc))
        {
            LogFlowThisFunc (("stopped mThread=%RTthrd\n", mThread));
            mThread = NIL_RTTHREAD;
            mTerminate = false;
        }
        else
        {
            AssertRC (rc);
            mLastError = rc;
        }
    }
    else
        LogFlowThisFunc (("not active\n"));

    return rc;
}


/**
 * Sort a list of USB devices.
 *
 * @returns Pointer to the head of the sorted doubly linked list.
 * @param   aDevices        Head pointer (can be both singly and doubly linked list).
 */
static PUSBDEVICE sortDevices (PUSBDEVICE pDevices)
{
    PUSBDEVICE pHead = NULL;
    PUSBDEVICE pTail = NULL;
    while (pDevices)
    {
        /* unlink head */
        PUSBDEVICE pDev = pDevices;
        pDevices = pDev->pNext;
        if (pDevices)
            pDevices->pPrev = NULL;

        /* find location. */
        PUSBDEVICE pCur = pTail;
        while (     pCur
               &&   HostUSBDevice::compare (pCur, pDev) > 0)
            pCur = pCur->pPrev;

        /* insert (after pCur) */
        pDev->pPrev = pCur;
        if (pCur)
        {
            pDev->pNext = pCur->pNext;
            pCur->pNext = pDev;
            if (pDev->pNext)
                pDev->pNext->pPrev = pDev;
            else
                pTail = pDev;
        }
        else
        {
            pDev->pNext = pHead;
            if (pHead)
                pHead->pPrev = pDev;
            else
                pTail = pDev;
            pHead = pDev;
        }
    }

    return pHead;
}


void USBProxyService::processChanges (void)
{
    LogFlowThisFunc (("\n"));

    /*
     * Get the sorted list of USB devices.
     */
    PUSBDEVICE pDevices = getDevices();
    if (pDevices)
    {
        pDevices = sortDevices (pDevices);

        /* we need to lock the host object for writing because
         * a) the subsequent code may call Host methods that require a write
         *    lock
         * b) we will lock HostUSBDevice objects below and want to make sure
         *    the lock order is always the same (Host, HostUSBDevice, as
         *    expected by Host) to avoid cross-deadlocks */

        AutoLock hostLock (mHost);

        /*
         * Compare previous list with the previous list of devices
         * and merge in any changes while notifying Host.
         */
        HostUSBDeviceList::iterator It = this->mDevices.begin();
        while (     It != mDevices.end()
               ||   pDevices)
        {
            ComObjPtr <HostUSBDevice> DevPtr;

            if (It != mDevices.end())
                DevPtr = *It;

            /* assert that the object is still alive (we still reference it in
             * the collection and we're the only one who calls uninit() on it */
            HostUSBDevice::AutoCaller devCaller (DevPtr.isNull() ? NULL : DevPtr);
            AssertComRC (devCaller.rc());

            /* Lock the device object since we will read/write it's
             * properties. All Host callbacks also imply the object is
             * locked. */
            AutoLock devLock (DevPtr.isNull() ? NULL : DevPtr);

            /*
             * Compare.
             */
            int iDiff;
            if (DevPtr.isNull())
                iDiff = 1;
            else
            {
                if (!pDevices)
                    iDiff = -1;
                else
                    iDiff = DevPtr->compare (pDevices);
            }
            if (!iDiff)
            {
                /*
                 * Device still there, update the state and move on.  Note
                 * that pDevices will be always adopted by the HostUSBDevice
                 * object (whose updateState() method must be always called by
                 * updateDeviceState()) and therefore there is no reason to
                 * free pDevices here.
                 */
                PUSBDEVICE pNext = pDevices->pNext; /* treated as singly linked */
                if (updateDeviceState (DevPtr, pDevices))
                {
                    Log (("USBProxyService::processChanges: state change %p:{.idVendor=%#06x, .idProduct=%#06x, .pszProduct=\"%s\", .pszManufacturer=\"%s\"} state=%d%s\n",
                          (HostUSBDevice *)DevPtr, pDevices->idVendor, pDevices->idProduct, pDevices->pszProduct, pDevices->pszManufacturer, DevPtr->state(), DevPtr->isStatePending() ? " (pending async op)" : ""));
                    mHost->onUSBDeviceStateChanged (DevPtr);
                }
                It++;
                pDevices = pNext;
            }
            else
            {
                if (iDiff > 0)
                {
                    /*
                     * Head of pDevices was attached.
                     */
                    PUSBDEVICE pNew = pDevices;
                    pDevices = pDevices->pNext;
                    pNew->pPrev = pNew->pNext = NULL;

                    ComObjPtr <HostUSBDevice> NewObj;
                    NewObj.createObject();
                    NewObj->init (pNew, this);
                    Log (("USBProxyService::processChanges: attached %p/%p:{.idVendor=%#06x, .idProduct=%#06x, .pszProduct=\"%s\", .pszManufacturer=\"%s\"}\n",
                          (HostUSBDevice *)NewObj, pNew, pNew->idVendor, pNew->idProduct, pNew->pszProduct, pNew->pszManufacturer));
                    deviceAdded (NewObj, pNew);

                    /* Not really necessary to lock here, but make Assert checks happy. */
                    AutoLock newDevLock (NewObj);

                    mDevices.insert (It, NewObj);
                    mHost->onUSBDeviceAttached (NewObj);
                }
                else
                {
                    /*
                     * DevPtr was detached, unless there is a pending async request.
                     */
                    /** @todo add a timeout here. */
                    if (!DevPtr->isStatePending())
                    {
                        It = mDevices.erase (It);
                        deviceRemoved (DevPtr);
                        mHost->onUSBDeviceDetached (DevPtr);
                        Log (("USBProxyService::processChanges: detached %p\n",
                              (HostUSBDevice *)DevPtr)); /** @todo add details .*/

                        /* from now on, the object is no more valid,
                         * uninitialize to avoid abuse */
                        devCaller.release();
                        DevPtr->uninit();
                    }
                    else
                    {
                        /* a state change (re-cycle) request is pending, go
                         * to the next device */
                        It++;
                        Log (("USBProxyService::processChanges: detached but pending %d %p\n",
                              DevPtr->pendingState(), (HostUSBDevice *)DevPtr));
                    }
                }
            }
        } /* while */
    }
    else
    {
        /* we need to lock the host object for writing because
         * a) the subsequent code may call Host methods that require a write
         *    lock
         * b) we will lock HostUSBDevice objects below and want to make sure
         *    the lock order is always the same (Host, HostUSBDevice, as
         *    expected by Host) to avoid cross-deadlocks */

        AutoLock hostLock (mHost);

        /* All devices were detached */
        HostUSBDeviceList::iterator It = this->mDevices.begin();
        while (It != mDevices.end())
        {
            ComObjPtr <HostUSBDevice> DevPtr = *It;

            /* assert that the object is still alive (we still reference it in
             * the collection and we're the only one who calls uninit() on it */
            HostUSBDevice::AutoCaller devCaller (DevPtr);
            AssertComRC (devCaller.rc());

            AutoLock devLock (DevPtr);

            /*
             * DevPtr was detached.
             */
            It = mDevices.erase (It);
            mHost->onUSBDeviceDetached (DevPtr);
            Log (("USBProxyService::processChanges: detached %p\n",
                  (HostUSBDevice *)DevPtr)); /** @todo add details .*/

            /* from now on, the object is no more valid,
             * uninitialize to avoid abuse */
            devCaller.release();
            DevPtr->uninit();
        }
    }

    LogFlowThisFunc (("returns void\n"));
}


/*static*/ DECLCALLBACK (int) USBProxyService::serviceThread (RTTHREAD Thread, void *pvUser)
{
    USBProxyService *pThis = (USBProxyService *)pvUser;
    LogFlowFunc (("pThis=%p\n", pThis));
    pThis->serviceThreadInit();

    /*
     * Processing loop.
     */
    for (;;)
    {
        pThis->wait (RT_INDEFINITE_WAIT);
        if (pThis->mTerminate)
            break;
        pThis->processChanges();
    }

    pThis->serviceThreadTerm();
    LogFlowFunc (("returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}


/*static*/ void USBProxyService::freeDeviceMembers (PUSBDEVICE pDevice)
{
    PUSBCONFIG pCfg = pDevice->paConfigurations;
    unsigned cCfgs = pDevice->bNumConfigurations;
    while (cCfgs-- > 0)
    {
        PUSBINTERFACE pIf = pCfg->paInterfaces;
        unsigned cIfs = pCfg->bNumInterfaces;
        while (cIfs-- > 0)
        {
            RTMemFree (pIf->paEndpoints);
            pIf->paEndpoints = NULL;
            RTStrFree ((char *)pIf->pszDriver);
            pIf->pszDriver = NULL;
            RTStrFree ((char *)pIf->pszInterface);
            pIf->pszInterface = NULL;
            /* next */
            pIf++;
        }
        RTMemFree (pCfg->paInterfaces);
        pCfg->paInterfaces = NULL;
        RTStrFree ((char *)pCfg->pszConfiguration);
        pCfg->pszConfiguration = NULL;

        /* next */
        pCfg++;
    }
    RTMemFree (pDevice->paConfigurations);
    pDevice->paConfigurations = NULL;

    RTStrFree ((char *)pDevice->pszManufacturer);
    pDevice->pszManufacturer = NULL;
    RTStrFree ((char *)pDevice->pszProduct);
    pDevice->pszProduct = NULL;
    RTStrFree ((char *)pDevice->pszSerialNumber);
    pDevice->pszSerialNumber = NULL;

    RTStrFree ((char *)pDevice->pszAddress);
    pDevice->pszAddress = NULL;
}

/*static*/ void USBProxyService::freeDevice (PUSBDEVICE pDevice)
{
    freeDeviceMembers (pDevice);
    RTMemFree (pDevice);
}


/* static */ uint64_t USBProxyService::calcSerialHash (const char *aSerial)
{
    if (!aSerial)
        aSerial = "";

    register const uint8_t *pu8 = (const uint8_t *)aSerial;
    register uint64_t u64 = 14695981039346656037ULL;
    for (;;)
    {
        register uint8_t u8 = *pu8;
        if (!u8)
            break;
        u64 = (u64 * 1099511628211ULL) ^ u8;
        pu8++;
    }

    return u64;
}


bool USBProxyService::updateDeviceStateFake (HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice)
{
    AssertReturn (aDevice, false);
    AssertReturn (aDevice->isLockedOnCurrentThread(), false);

    if (aDevice->isStatePending())
    {
        switch (aDevice->pendingState())
        {
            /* @todo USBDEVICESTATE_USED_BY_GUEST seems not to be used anywhere in the proxy code; it's
             * quite logical because the proxy doesn't know anything about guest VMs. We use HELD_BY_PROXY
             * instead -- it is sufficient and is what Main expects. */
            case USBDeviceState_USBDeviceCaptured:      aUSBDevice->enmState = USBDEVICESTATE_HELD_BY_PROXY; break;
            case USBDeviceState_USBDeviceHeld:          aUSBDevice->enmState = USBDEVICESTATE_HELD_BY_PROXY; break;
            case USBDeviceState_USBDeviceAvailable:     aUSBDevice->enmState = USBDEVICESTATE_UNUSED; break;
            case USBDeviceState_USBDeviceUnavailable:   aUSBDevice->enmState = USBDEVICESTATE_USED_BY_HOST; break;
            case USBDeviceState_USBDeviceBusy:          aUSBDevice->enmState = USBDEVICESTATE_USED_BY_HOST_CAPTURABLE; break;
            default:
                AssertMsgFailed(("%d\n", aDevice->pendingState()));
                break;
        }
    }

    return USBProxyService::updateDeviceState (aDevice, aUSBDevice);
}



/* Stubs which the host specific classes overrides: */


int USBProxyService::wait (unsigned aMillies)
{
    return RTThreadSleep (250);
}


int USBProxyService::interruptWait (void)
{
    return VERR_NOT_IMPLEMENTED;
}


PUSBDEVICE USBProxyService::getDevices (void)
{
    return NULL;
}


void USBProxyService::serviceThreadInit (void)
{
}


void USBProxyService::serviceThreadTerm (void)
{
}


/**
 *  The default implementation returns non-NULL to emulate successful insertions
 *  for those subclasses that don't reimplement this method.
 */
void *USBProxyService::insertFilter (IUSBDeviceFilter * /* aFilter */)
{
    // return non-NULL to prevent failed assertions in Main
    return (void *) 1;
}


void USBProxyService::removeFilter (void * /* aID */)
{
}


int USBProxyService::captureDevice (HostUSBDevice * /* aDevice */)
{
    return VERR_NOT_IMPLEMENTED;
}


int USBProxyService::holdDevice (HostUSBDevice * /* aDevice */)
{
    return VERR_NOT_IMPLEMENTED;
}


int USBProxyService::releaseDevice (HostUSBDevice * /* aDevice */)
{
    return VERR_NOT_IMPLEMENTED;
}


int USBProxyService::resetDevice (HostUSBDevice * /* aDevice */)
{
    return VERR_NOT_IMPLEMENTED;
}


bool USBProxyService::updateDeviceState (HostUSBDevice *aDevice, PUSBDEVICE aUSBDevice)
{
    AssertReturn (aDevice, false);
    AssertReturn (aDevice->isLockedOnCurrentThread(), false);

    return aDevice->updateState (aUSBDevice);
}


void USBProxyService::deviceAdded (HostUSBDevice * /* aDevice */, PUSBDEVICE /* aUSBDevice */)
{
}


void USBProxyService::deviceRemoved (HostUSBDevice * /* aDevice */)
{
}

