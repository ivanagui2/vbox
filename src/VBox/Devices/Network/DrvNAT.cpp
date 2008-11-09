/** @file
 *
 * VBox network devices:
 * NAT network transport driver
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_NAT
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#ifndef VBOX_NAT_SOURCES
#include "Network/slirp/libslirp.h"
#else
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>

#include <unistd.h>

#include <fcntl.h>

#include <string.h>

#endif
#include <VBox/pdmdrv.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/critsect.h>
#include <iprt/cidr.h>

#include "Builtins.h"

#ifdef VBOX_NAT_SOURCES
#include "Network/nat/nat.h"
#endif
#ifdef VBOX_WITH_SIMPLEFIED_SLIRP_SYNC
#include <unistd.h>
#include <errno.h>
#include<iprt/semaphore.h>
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * NAT network transport driver instance data.
 */
typedef struct DRVNAT
{
    /** The network interface. */
    PDMINETWORKCONNECTOR    INetworkConnector;
    /** The port we're attached to. */
    PPDMINETWORKPORT        pPort;
    /** The network config of the port we're attached to. */
    PPDMINETWORKCONFIG      pConfig;
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
#ifndef VBOX_WITH_SIMPLEFIED_SLIRP_SYNC
    /** Slirp critical section. */
    RTCRITSECT              CritSect;
#endif
    /** Link state */
    PDMNETWORKLINKSTATE     enmLinkState;
    /** NAT state for this instance. */
#ifndef VBOX_NAT_SOURCES
    PNATState               pNATState;
#endif
    /** TFTP directory prefix. */
    char                    *pszTFTPPrefix;
    /** Boot file name to provide in the DHCP server response. */
    char                    *pszBootFile;
#ifdef VBOX_WITH_SIMPLEFIED_SLIRP_SYNC
    /*polling thread*/
    PPDMTHREAD              pThread;
    /*used for wakep of poling thread*/
    RTSEMEVENT               semIOmutex;
    RTSEMEVENT           semSndMutex;
#ifndef RT_OS_WINDOWS
    /** The write end of the control pipe. */
    RTFILE                  PipeWrite;
    /** The read end of the control pipe. */
    RTFILE                  PipeRead;
#else
#endif
    char                   cBuffer[1600];
    size_t                  sBufferSize;
#endif
} DRVNAT, *PDRVNAT;

/** Converts a pointer to NAT::INetworkConnector to a PRDVNAT. */
#define PDMINETWORKCONNECTOR_2_DRVNAT(pInterface)   ( (PDRVNAT)((uintptr_t)pInterface - RT_OFFSETOF(DRVNAT, INetworkConnector)) )


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#if 0
/** If set the thread should terminate. */
static bool             g_fThreadTerm = false;
/** The thread id of the select thread (drvNATSelectThread()). */
static RTTHREAD         g_ThreadSelect;
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/


#ifdef VBOX_NAT_SOURCES
/*
 * Sends data to guest called from NAT glue code
 */
static DECLCALLBACK(void) drvNATOutput(const void * data, const uint8_t *msg, int size)
{
        PDRVNAT pThis = (PDRVNAT)(void *)data;
        LogFlow(("output: pvBuf=%p cb=%#x\n", msg, size));
        int rc =  pThis->pPort->pfnWaitReceiveAvail(pThis->pPort, 0);
        if (RT_SUCCESS(rc))
                pThis->pPort->pfnReceive(pThis->pPort, msg, size);
        LogFlow(("output: exit\n"));
}

#endif

/**
 * Send data to the network.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pvBuf           Data to send.
 * @param   cb              Number of bytes to send.
 * @thread  EMT
 */
static DECLCALLBACK(int) drvNATSend(PPDMINETWORKCONNECTOR pInterface, const void *pvBuf, size_t cb)
{
    PDRVNAT pThis = PDMINETWORKCONNECTOR_2_DRVNAT(pInterface);

    LogFlow(("drvNATSend: pvBuf=%p cb=%#x\n", pvBuf, cb));
    Log2(("drvNATSend: pvBuf=%p cb=%#x\n"
          "%.*Vhxd\n",
          pvBuf, cb, cb, pvBuf));

#ifndef VBOX_WITH_SIMPLEFIED_SLIRP_SYNC
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertReleaseRC(rc);
#else
    /*notify select to wakeup*/
    memcpy(pThis->cBuffer,pvBuf, cb);
    pThis->sBufferSize = cb;
    int rc = RTFileWrite(pThis->PipeWrite, "", 1, NULL);
    AssertRC(rc);
    RTSemEventWait(pThis->semSndMutex, RT_INDEFINITE_WAIT);
#endif

#ifndef VBOX_WITH_SIMPLEFIED_SLIRP_SYNC
    Assert(pThis->enmLinkState == PDMNETWORKLINKSTATE_UP);
    if (pThis->enmLinkState == PDMNETWORKLINKSTATE_UP) {
#ifndef VBOX_NAT_SOURCES
        slirp_input(pThis->pNATState, (uint8_t *)pvBuf, cb);
#else
        ether_chk(pThis, pvBuf, cb);
#endif
    }
    RTCritSectLeave(&pThis->CritSect);
#endif
    LogFlow(("drvNATSend: end\n"));
    return VINF_SUCCESS;
}


/**
 * Set promiscuous mode.
 *
 * This is called when the promiscuous mode is set. This means that there doesn't have
 * to be a mode change when it's called.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   fPromiscuous    Set if the adaptor is now in promiscuous mode. Clear if it is not.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvNATSetPromiscuousMode(PPDMINETWORKCONNECTOR pInterface, bool fPromiscuous)
{
    LogFlow(("drvNATSetPromiscuousMode: fPromiscuous=%d\n", fPromiscuous));
    /* nothing to do */
}


/**
 * Notification on link status changes.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   enmLinkState    The new link state.
 * @thread  EMT
 */
static DECLCALLBACK(void) drvNATNotifyLinkChanged(PPDMINETWORKCONNECTOR pInterface, PDMNETWORKLINKSTATE enmLinkState)
{
    PDRVNAT pThis = PDMINETWORKCONNECTOR_2_DRVNAT(pInterface);

    LogFlow(("drvNATNotifyLinkChanged: enmLinkState=%d\n", enmLinkState));

#ifndef VBOX_WITH_SIMPLEFIED_SLIRP_SYNC
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertReleaseRC(rc);
#endif
    pThis->enmLinkState = enmLinkState;

    switch (enmLinkState)
    {
        case PDMNETWORKLINKSTATE_UP:
            LogRel(("NAT: link up\n"));
#ifndef VBOX_NAT_SOURCES
            slirp_link_up(pThis->pNATState);
#endif
            break;

        case PDMNETWORKLINKSTATE_DOWN:
        case PDMNETWORKLINKSTATE_DOWN_RESUME:
            LogRel(("NAT: link down\n"));
#ifndef VBOX_NAT_SOURCES
            slirp_link_down(pThis->pNATState);
#endif
            break;

        default:
            AssertMsgFailed(("drvNATNotifyLinkChanged: unexpected link state %d\n", enmLinkState));
    }
#ifndef VBOX_WITH_SIMPLEFIED_SLIRP_SYNC
    RTCritSectLeave(&pThis->CritSect);
#endif
}


#ifndef VBOX_WITH_SIMPLEFIED_SLIRP_SYNC
/**
 * Poller callback.
 */
static DECLCALLBACK(void) drvNATPoller(PPDMDRVINS pDrvIns)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    fd_set  ReadFDs;
    fd_set  WriteFDs;
    fd_set  XcptFDs;
    int     cFDs = -1;
    FD_ZERO(&ReadFDs);
    FD_ZERO(&WriteFDs);
    FD_ZERO(&XcptFDs);

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertReleaseRC(rc);

#ifndef VBOX_NAT_SOURCES
    slirp_select_fill(pThis->pNATState, &cFDs, &ReadFDs, &WriteFDs, &XcptFDs);
#else
    nat_select_fill(NULL, &cFDs, &ReadFDs, &WriteFDs, &XcptFDs);
#endif

    struct timeval tv = {0, 0}; /* no wait */
    int cReadFDs = select(cFDs + 1, &ReadFDs, &WriteFDs, &XcptFDs, &tv);
#ifndef VBOX_NAT_SOURCES
    if (cReadFDs >= 0)
        slirp_select_poll(pThis->pNATState, &ReadFDs, &WriteFDs, &XcptFDs);
#else
    if (cReadFDs >= 0) {
        nat_select_poll(pThis, &ReadFDs, &WriteFDs, &XcptFDs);
    }
#endif

    RTCritSectLeave(&pThis->CritSect);
}
#else

static DECLCALLBACK(int) drvNATAsyncIoThread(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    fd_set  ReadFDs;
    fd_set  WriteFDs;
    fd_set  XcptFDs;
    int     cFDs = -1;
    int     rc;

    LogFlow(("drvNATAsyncIoThread: pThis=%p\n", pThis));


    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;
    /*
     * Polling loop.
     */
    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        FD_ZERO(&ReadFDs);
        FD_ZERO(&WriteFDs);
        FD_ZERO(&XcptFDs);
        cFDs = -1;

        /*
         * To prevent concurent execution of sending/receving threads
         */
        slirp_select_fill(pThis->pNATState, &cFDs, &ReadFDs, &WriteFDs, &XcptFDs);

        struct timeval tv = {0, 200}; /* 2 millis wait */

        FD_SET(pThis->PipeRead, &ReadFDs); /*Linux only*/
        cFDs = (pThis->PipeRead < cFDs ? cFDs:pThis->PipeRead);
        int cReadFDs = select(cFDs + 1, &ReadFDs, &WriteFDs, &XcptFDs, &tv);

        if (cReadFDs >= 0) {
            slirp_select_poll(pThis->pNATState, &ReadFDs, &WriteFDs, &XcptFDs);

            if (FD_ISSET(pThis->PipeRead, &ReadFDs)) {
                /* drain the pipe */
                char ch;
                size_t cbRead;
                RTFileRead(pThis->PipeRead, &ch, 1, &cbRead);
                slirp_input(pThis->pNATState, (uint8_t *)pThis->cBuffer, pThis->sBufferSize);
                RTSemEventSignal(pThis->semSndMutex);
            }
        }
#if 0
        if (cReadFDs == 0) {
            rc = RTSemEventWait(pThis->semIOmutex, RT_INDEFINITE_WAIT);
            AssertReleaseRC(rc);
        }
#endif
    }

    return VINF_SUCCESS;
}

 /**
 *  Unblock the send thread so it can respond to a state change.
 *
 *  @returns VBox status code.
 *  @param   pDevIns     The pcnet device instance.
 *  @param   pThread     The send thread.
 */
static DECLCALLBACK(int) drvNATAsyncIoWakeup(PPDMDRVINS pDrvIns, PPDMTHREAD pThread)
{
#if 0
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    int rc = RTSemEventSignal(pThis->semIOmutex);
    AssertReleaseRC(rc);
#endif
    return (VINF_SUCCESS);
}

#endif

#ifndef VBOX_NAT_SOURCES
/**
 * Function called by slirp to check if it's possible to feed incoming data to the network port.
 * @returns 1 if possible.
 * @returns 0 if not possible.
 */
int slirp_can_output(void *pvUser)
{
    PDRVNAT pThis = (PDRVNAT)pvUser;

    Assert(pThis);

#ifndef VBOX_WITH_SIMPLEFIED_SLIRP_SYNC
    /** Happens during termination */
    if (!RTCritSectIsOwner(&pThis->CritSect))
        return 0;
#endif

    int rc =  pThis->pPort->pfnWaitReceiveAvail(pThis->pPort, 0);
    return RT_SUCCESS(rc);
}


/**
 * Function called by slirp to feed incoming data to the network port.
 */
void slirp_output(void *pvUser, const uint8_t *pu8Buf, int cb)
{
    PDRVNAT pThis = (PDRVNAT)pvUser;

    LogFlow(("slirp_output BEGIN %x %d\n", pu8Buf, cb));
    Log2(("slirp_output: pu8Buf=%p cb=%#x (pThis=%p)\n"
          "%.*Vhxd\n",
          pu8Buf, cb, pThis,
          cb, pu8Buf));

    Assert(pThis);

#ifndef VBOX_WITH_SIMPLEFIED_SLIRP_SYNC
    /** Happens during termination */
    if (!RTCritSectIsOwner(&pThis->CritSect))
        return;
#endif

    int rc = pThis->pPort->pfnReceive(pThis->pPort, pu8Buf, cb);
    AssertRC(rc);
    LogFlow(("slirp_output END %x %d\n", pu8Buf, cb));
}
#endif

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  Any thread.
 */
static DECLCALLBACK(void *) drvNATQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_NETWORK_CONNECTOR:
            return &pThis->INetworkConnector;
        default:
            return NULL;
    }
}


/**
 * Destruct a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvNATDestruct(PPDMDRVINS pDrvIns)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);

    LogFlow(("drvNATDestruct:\n"));

#ifndef VBOX_WITH_SIMPLEFIED_SLIRP_SYNC
    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertReleaseRC(rc);
#endif
#ifndef VBOX_NAT_SOURCES
    slirp_term(pThis->pNATState);
    pThis->pNATState = NULL;
#endif
#ifndef VBOX_WITH_SIMPLEFIED_SLIRP_SYNC
    RTCritSectLeave(&pThis->CritSect);

    RTCritSectDelete(&pThis->CritSect);
#else
    RTSemEventDestroy(pThis->semSndMutex);
#endif
}


/**
 * Sets up the redirectors.
 *
 * @returns VBox status code.
 * @param   pCfgHandle      The drivers configuration handle.
 */
static int drvNATConstructRedir(unsigned iInstance, PDRVNAT pThis, PCFGMNODE pCfgHandle, RTIPV4ADDR Network)
{
#ifndef VBOX_NAT_SOURCES
    /*
     * Enumerate redirections.
     */
    for (PCFGMNODE pNode = CFGMR3GetFirstChild(pCfgHandle); pNode; pNode = CFGMR3GetNextChild(pNode))
    {
        /*
         * Validate the port forwarding config.
         */
        if (!CFGMR3AreValuesValid(pNode, "Protocol\0UDP\0HostPort\0GuestPort\0GuestIP\0"))
            return PDMDRV_SET_ERROR(pThis->pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES, N_("Unknown configuration in port forwarding"));

        /* protocol type */
        bool fUDP;
        char szProtocol[32];
        int rc = CFGMR3QueryString(pNode, "Protocol", &szProtocol[0], sizeof(szProtocol));
        if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        {
            rc = CFGMR3QueryBool(pNode, "UDP", &fUDP);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                fUDP = false;
            else if (RT_FAILURE(rc))
                return PDMDrvHlpVMSetError(pThis->pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"UDP\" boolean failed"), iInstance);
        }
        else if (RT_SUCCESS(rc))
        {
            if (!RTStrICmp(szProtocol, "TCP"))
                fUDP = false;
            else if (!RTStrICmp(szProtocol, "UDP"))
                fUDP = true;
            else
                return PDMDrvHlpVMSetError(pThis->pDrvIns, VERR_INVALID_PARAMETER, RT_SRC_POS, N_("NAT#%d: Invalid configuration value for \"Protocol\": \"%s\""), iInstance, szProtocol);
        }
        else
            return PDMDrvHlpVMSetError(pThis->pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"Protocol\" string failed"), iInstance);

        /* host port */
        int32_t iHostPort;
        rc = CFGMR3QueryS32(pNode, "HostPort", &iHostPort);
        if (RT_FAILURE(rc))
            return PDMDrvHlpVMSetError(pThis->pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"HostPort\" integer failed"), iInstance);

        /* guest port */
        int32_t iGuestPort;
        rc = CFGMR3QueryS32(pNode, "GuestPort", &iGuestPort);
        if (RT_FAILURE(rc))
            return PDMDrvHlpVMSetError(pThis->pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"GuestPort\" integer failed"), iInstance);

        /* guest address */
        char    szGuestIP[32];
        rc = CFGMR3QueryString(pNode, "GuestIP", &szGuestIP[0], sizeof(szGuestIP));
        if (rc == VERR_CFGM_VALUE_NOT_FOUND)
            RTStrPrintf(szGuestIP, sizeof(szGuestIP), "%d.%d.%d.%d",
                        (Network & 0xFF000000) >> 24, (Network & 0xFF0000) >> 16, (Network & 0xFF00) >> 8, (Network & 0xE0) | 15);
        else if (RT_FAILURE(rc))
            return PDMDrvHlpVMSetError(pThis->pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"GuestIP\" string failed"), iInstance);
        struct in_addr GuestIP;
        if (!inet_aton(szGuestIP, &GuestIP))
            return PDMDrvHlpVMSetError(pThis->pDrvIns, VERR_NAT_REDIR_GUEST_IP, RT_SRC_POS, N_("NAT#%d: configuration error: invalid \"GuestIP\"=\"%s\", inet_aton failed"), iInstance, szGuestIP);

        /*
         * Call slirp about it.
         */
        Log(("drvNATConstruct: Redir %d -> %s:%d\n", iHostPort, szGuestIP, iGuestPort));
        if (slirp_redir(pThis->pNATState, fUDP, iHostPort, GuestIP, iGuestPort) < 0)
            return PDMDrvHlpVMSetError(pThis->pDrvIns, VERR_NAT_REDIR_SETUP, RT_SRC_POS, N_("NAT#%d: configuration error: failed to set up redirection of %d to %s:%d. Probably a conflict with existing services or other rules"), iInstance, iHostPort, szGuestIP, iGuestPort);
    } /* for each redir rule */
#endif

    return VINF_SUCCESS;
}

/**
 * Get the MAC address into the slirp stack.
 */
static void drvNATSetMac(PDRVNAT pThis)
{
#ifndef VBOX_NAT_SOURCES
    if (pThis->pConfig)
    {
        RTMAC Mac;
        pThis->pConfig->pfnGetMac(pThis->pConfig, &Mac);
        slirp_set_ethaddr(pThis->pNATState, Mac.au8);
    }
#endif
}


/**
 * After loading we have to pass the MAC address of the ethernet device to the slirp stack.
 * Otherwise the guest is not reachable until it performs a DHCP request or an ARP request
 * (usually done during guest boot).
 */
static DECLCALLBACK(int) drvNATLoadDone(PPDMDRVINS pDrvIns, PSSMHANDLE pSSMHandle)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    drvNATSetMac(pThis);
    return VINF_SUCCESS;
}


/**
 * Some guests might not use DHCP to retrieve an IP but use a static IP.
 */
static DECLCALLBACK(void) drvNATPowerOn(PPDMDRVINS pDrvIns)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    drvNATSetMac(pThis);
}


/**
 * Construct a NAT network transport driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) drvNATConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVNAT pThis = PDMINS_2_DATA(pDrvIns, PDRVNAT);
    char szNetAddr[16];
    char szNetwork[32]; /* xxx.xxx.xxx.xxx/yy */
    LogFlow(("drvNATConstruct:\n"));

    /*
     * Validate the config.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "PassDomain\0TFTPPrefix\0BootFile\0Network\0"))
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES, N_("Unknown NAT configuration option, only supports PassDomain, TFTPPrefix, BootFile and Network"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                      = pDrvIns;
#ifndef VBOX_NAT_SOURCES
    pThis->pNATState                    = NULL;
#endif
    pThis->pszTFTPPrefix                = NULL;
    pThis->pszBootFile                  = NULL;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface    = drvNATQueryInterface;
    /* INetwork */
    pThis->INetworkConnector.pfnSend               = drvNATSend;
    pThis->INetworkConnector.pfnSetPromiscuousMode = drvNATSetPromiscuousMode;
    pThis->INetworkConnector.pfnNotifyLinkChanged  = drvNATNotifyLinkChanged;

    /*
     * Get the configuration settings.
     */
    bool fPassDomain = true;
    int rc = CFGMR3QueryBool(pCfgHandle, "PassDomain", &fPassDomain);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fPassDomain = true;
    else if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"PassDomain\" boolean failed"), pDrvIns->iInstance);

    rc = CFGMR3QueryStringAlloc(pCfgHandle, "TFTPPrefix", &pThis->pszTFTPPrefix);
    if (RT_FAILURE(rc) && rc != VERR_CFGM_VALUE_NOT_FOUND)
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"TFTPPrefix\" string failed"), pDrvIns->iInstance);
    rc = CFGMR3QueryStringAlloc(pCfgHandle, "BootFile", &pThis->pszBootFile);
    if (RT_FAILURE(rc) && rc != VERR_CFGM_VALUE_NOT_FOUND)
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"BootFile\" string failed"), pDrvIns->iInstance);

    /*
     * Query the network port interface.
     */
    pThis->pPort = (PPDMINETWORKPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_NETWORK_PORT);
    if (!pThis->pPort)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
                                N_("Configuration error: the above device/driver didn't export the network port interface"));
    pThis->pConfig = (PPDMINETWORKCONFIG)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_NETWORK_CONFIG);
    if (!pThis->pConfig)
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_ABOVE,
                                N_("Configuration error: the above device/driver didn't export the network config interface"));

    /* Generate a network address for this network card. */
    rc = CFGMR3QueryString(pCfgHandle, "Network", szNetwork, sizeof(szNetwork));
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        RTStrPrintf(szNetwork, sizeof(szNetwork), "10.0.%d.0/24", pDrvIns->iInstance + 2);
    else if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: configuration query for \"Network\" string failed"), pDrvIns->iInstance);

    RTIPV4ADDR Network;
    RTIPV4ADDR Netmask;
    rc = RTCidrStrToIPv4(szNetwork, &Network, &Netmask);
    if (RT_FAILURE(rc))
        return PDMDrvHlpVMSetError(pDrvIns, rc, RT_SRC_POS, N_("NAT#%d: Configuration error: network '%s' describes not a valid IPv4 network"), pDrvIns->iInstance, szNetwork);

    RTStrPrintf(szNetAddr, sizeof(szNetAddr), "%d.%d.%d.%d",
               (Network & 0xFF000000) >> 24, (Network & 0xFF0000) >> 16, (Network & 0xFF00) >> 8, Network & 0xFF);

    /*
     * The slirp lock..
     */
#ifndef VBOX_WITH_SIMPLEFIED_SLIRP_SYNC
    rc = RTCritSectInit(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;
#endif
#if 0
    rc = RTSemEventCreate(&g_EventSem);
    if (RT_SUCCESS(rc))
    {
        /*
         * Start the select thread. (it'll block on the sem)
         */
        g_fThreadTerm = false;
        rc = RTThreadCreate(&g_ThreadSelect, drvNATSelectThread, 0, NULL, "NATSEL");
        if (RT_SUCCESS(rc))
        {
#endif
#ifndef VBOX_NAT_SOURCES
            /*
             * Initialize slirp.
             */
            rc = slirp_init(&pThis->pNATState, &szNetAddr[0], Netmask, fPassDomain, pThis->pszTFTPPrefix, pThis->pszBootFile, pThis);
            if (RT_SUCCESS(rc))
            {
                int rc2 = drvNATConstructRedir(pDrvIns->iInstance, pThis, pCfgHandle, Network);
                if (RT_SUCCESS(rc2))
                {
                    /*
                     * Register a load done notification to get the MAC address into the slirp
                     * engine after we loaded a guest state.
                     */
                    rc2 = PDMDrvHlpSSMRegister(pDrvIns, pDrvIns->pDrvReg->szDriverName,
                                               pDrvIns->iInstance, 0, 0,
                                               NULL, NULL, NULL, NULL, NULL, drvNATLoadDone);
                    AssertRC(rc2);
#ifndef VBOX_WITH_SIMPLEFIED_SLIRP_SYNC
                    pDrvIns->pDrvHlp->pfnPDMPollerRegister(pDrvIns, drvNATPoller);
#else

                    rc = RTSemEventCreate(&pThis->semIOmutex);
                    AssertReleaseRC(rc);
                    rc = RTSemEventCreate(&pThis->semSndMutex);
                    AssertReleaseRC(rc);

                    /*
                     * Create the control pipe.
                     * XXX: Linux only
                     */
                    int fds[2];
                    if (pipe(&fds[0]) != 0) /** @todo RTPipeCreate() or something... */
                    {
                        int rc = RTErrConvertFromErrno(errno);
                        AssertRC(rc);
                        return rc;
                    }
                    pThis->PipeRead = fds[0];
                    pThis->PipeWrite = fds[1];

                    rc = PDMDrvHlpPDMThreadCreate(pDrvIns, &pThis->pThread, pThis, drvNATAsyncIoThread, drvNATAsyncIoWakeup, 128 * _1K, RTTHREADTYPE_IO, "NAT");
                    AssertReleaseRC(rc);
#endif

                    pThis->enmLinkState = PDMNETWORKLINKSTATE_UP;
#if 0
                    RTSemEventSignal(g_EventSem);
                    RTThreadSleep(0);
#endif
                    /* might return VINF_NAT_DNS */
                    return rc;
                }
                /* failure path */
                rc = rc2;
                slirp_term(pThis->pNATState);
                pThis->pNATState = NULL;
            }
            else
            {
              PDMDRV_SET_ERROR(pDrvIns, rc, N_("Unknown error during NAT networking setup: "));
              AssertMsgFailed(("Add error message for rc=%d (%Rrc)\n", rc, rc));
            }
#else
        pDrvIns->pDrvHlp->pfnPDMPollerRegister(pDrvIns, drvNATPoller);
        pThis->enmLinkState = PDMNETWORKLINKSTATE_UP;
        struct nat_output_callbacks cb;
        cb.noc_guest_out = drvNATOutput;
        nat_init(&cb, pDrvIns);
#endif
#if 0
            g_fThreadTerm = true;
            RTSemEventSignal(g_EventSem);
            RTThreadSleep(0);
        }
        RTSemEventDestroy(g_EventSem);
        g_EventSem = NULL;
    }
#endif
#ifndef VBOX_WITH_SIMPLEFIED_SLIRP_SYNC
    RTCritSectDelete(&pThis->CritSect);
#endif
    return rc;
}




/**
 * NAT network transport driver registration record.
 */
const PDMDRVREG g_DrvNAT =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "NAT",
    /* pszDescription */
    "NAT Network Transport Driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_NETWORK,
    /* cMaxInstances */
    16,
    /* cbInstance */
    sizeof(DRVNAT),
    /* pfnConstruct */
    drvNATConstruct,
    /* pfnDestruct */
    drvNATDestruct,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    drvNATPowerOn,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL
};
