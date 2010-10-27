/* $Id$ */
/** @file
 * RawHDDCore - Raw Disk image, Core Code.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VD_RAW
#include <VBox/VBoxHDD-Plugin.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/path.h>

/*******************************************************************************
*   Constants And Macros, Structures and Typedefs                              *
*******************************************************************************/
/**
 * ISO volume descriptor.
 */
#pragma pack(1)
typedef struct ISOVOLDESC
{
    union
    {
        /** Byte view. */
        uint8_t au8[2048];
        /** Field view. */
        struct
        {
            /** Descriptor type. */
            uint8_t u8Type;
            /** Standard identifier */
            uint8_t au8Id[5];
            /** Descriptor version. */
            uint8_t u8Version;
            /** Rest depends on the descriptor type. */
        } fields;
    };
} ISOVOLDESC, *PISOVOLDESC;
#pragma pack()
AssertCompileSize(ISOVOLDESC, 2048);

/**
 * Raw image data structure.
 */
typedef struct RAWIMAGE
{
    /** Image name. */
    const char          *pszFilename;
    /** Storage handle. */
    PVDIOSTORAGE        pStorage;
    /** I/O interface. */
    PVDINTERFACE        pInterfaceIO;
    /** Async I/O interface callbacks. */
    PVDINTERFACEIOINT   pInterfaceIOCallbacks;

    /** Pointer to the per-disk VD interface list. */
    PVDINTERFACE        pVDIfsDisk;
    /** Pointer to the per-image VD interface list. */
    PVDINTERFACE        pVDIfsImage;

    /** Error callback. */
    PVDINTERFACE        pInterfaceError;
    /** Opaque data for error callback. */
    PVDINTERFACEERROR   pInterfaceErrorCallbacks;

    /** Open flags passed by VBoxHD layer. */
    unsigned            uOpenFlags;
    /** Image flags defined during creation or determined during open. */
    unsigned            uImageFlags;
    /** Total size of the image. */
    uint64_t            cbSize;
    /** Position in the image (only truly used for sequential access). */
    uint64_t            offAccess;
    /** Flag if this is a newly created image. */
    bool                fCreate;
    /** Physical geometry of this image. */
    VDGEOMETRY          PCHSGeometry;
    /** Logical geometry of this image. */
    VDGEOMETRY          LCHSGeometry;

} RAWIMAGE, *PRAWIMAGE;


/** Size of write operations when filling an image with zeroes. */
#define RAW_FILL_SIZE (128 * _1K)

/*******************************************************************************
*   Static Variables                                                           *
*******************************************************************************/

/** NULL-terminated array of supported file extensions. */
static const VDFILEEXTENSION s_aRawFileExtensions[] =
{
    {"iso", VDTYPE_DVD},
    {"img", VDTYPE_FLOPPY},
    {"ima", VDTYPE_FLOPPY},
    {NULL, VDTYPE_INVALID}
};

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

/**
 * Internal: signal an error to the frontend.
 */
DECLINLINE(int) rawError(PRAWIMAGE pImage, int rc, RT_SRC_POS_DECL,
                         const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    if (pImage->pInterfaceError)
        pImage->pInterfaceErrorCallbacks->pfnError(pImage->pInterfaceError->pvUser, rc, RT_SRC_POS_ARGS,
                                                   pszFormat, va);
    va_end(va);
    return rc;
}

/**
 * Internal: signal an informational message to the frontend.
 */
DECLINLINE(int) rawMessage(PRAWIMAGE pImage, const char *pszFormat, ...)
{
    int rc = VINF_SUCCESS;
    va_list va;
    va_start(va, pszFormat);
    if (pImage->pInterfaceError)
        rc = pImage->pInterfaceErrorCallbacks->pfnMessage(pImage->pInterfaceError->pvUser,
                                                          pszFormat, va);
    va_end(va);
    return rc;
}


DECLINLINE(int) rawFileOpen(PRAWIMAGE pImage, const char *pszFilename,
                            uint32_t fOpen)
{
    return pImage->pInterfaceIOCallbacks->pfnOpen(pImage->pInterfaceIO->pvUser,
                                                  pszFilename, fOpen,
                                                  &pImage->pStorage);
}

DECLINLINE(int) rawFileClose(PRAWIMAGE pImage)
{
    return pImage->pInterfaceIOCallbacks->pfnClose(pImage->pInterfaceIO->pvUser,
                                                   pImage->pStorage);
}

DECLINLINE(int) rawFileDelete(PRAWIMAGE pImage, const char *pszFilename)
{
    return pImage->pInterfaceIOCallbacks->pfnDelete(pImage->pInterfaceIO->pvUser,
                                                    pszFilename);
}

DECLINLINE(int) rawFileMove(PRAWIMAGE pImage, const char *pszSrc,
                            const char *pszDst, unsigned fMove)
{
    return pImage->pInterfaceIOCallbacks->pfnMove(pImage->pInterfaceIO->pvUser,
                                                  pszSrc, pszDst, fMove);
}

DECLINLINE(int) rawFileGetFreeSpace(PRAWIMAGE pImage, const char *pszFilename,
                                    int64_t *pcbFree)
{
    return pImage->pInterfaceIOCallbacks->pfnGetFreeSpace(pImage->pInterfaceIO->pvUser,
                                                          pszFilename, pcbFree);
}

DECLINLINE(int) rawFileGetSize(PRAWIMAGE pImage, uint64_t *pcbSize)
{
    return pImage->pInterfaceIOCallbacks->pfnGetSize(pImage->pInterfaceIO->pvUser,
                                                     pImage->pStorage, pcbSize);
}

DECLINLINE(int) rawFileSetSize(PRAWIMAGE pImage, uint64_t cbSize)
{
    return pImage->pInterfaceIOCallbacks->pfnSetSize(pImage->pInterfaceIO->pvUser,
                                                     pImage->pStorage, cbSize);
}

DECLINLINE(int) rawFileWriteSync(PRAWIMAGE pImage, uint64_t uOffset,
                                 const void *pvBuffer, size_t cbBuffer,
                                 size_t *pcbWritten)
{
    return pImage->pInterfaceIOCallbacks->pfnWriteSync(pImage->pInterfaceIO->pvUser,
                                                       pImage->pStorage, uOffset,
                                                       pvBuffer, cbBuffer, pcbWritten);
}

DECLINLINE(int) rawFileReadSync(PRAWIMAGE pImage, uint64_t uOffset,
                                void *pvBuffer, size_t cbBuffer, size_t *pcbRead)
{
    return pImage->pInterfaceIOCallbacks->pfnReadSync(pImage->pInterfaceIO->pvUser,
                                                      pImage->pStorage, uOffset,
                                                      pvBuffer, cbBuffer, pcbRead);
}

DECLINLINE(int) rawFileFlushSync(PRAWIMAGE pImage)
{
    return pImage->pInterfaceIOCallbacks->pfnFlushSync(pImage->pInterfaceIO->pvUser,
                                                       pImage->pStorage);
}

DECLINLINE(int) rawFileReadUserAsync(PRAWIMAGE pImage, uint64_t uOffset,
                                     PVDIOCTX pIoCtx, size_t cbRead)
{
    return pImage->pInterfaceIOCallbacks->pfnReadUserAsync(pImage->pInterfaceIO->pvUser,
                                                           pImage->pStorage,
                                                           uOffset, pIoCtx,
                                                           cbRead);
}

DECLINLINE(int) rawFileWriteUserAsync(PRAWIMAGE pImage, uint64_t uOffset,
                                      PVDIOCTX pIoCtx, size_t cbWrite,
                                      PFNVDXFERCOMPLETED pfnComplete,
                                      void *pvCompleteUser)
{
    return pImage->pInterfaceIOCallbacks->pfnWriteUserAsync(pImage->pInterfaceIO->pvUser,
                                                            pImage->pStorage,
                                                            uOffset, pIoCtx,
                                                            cbWrite,
                                                            pfnComplete,
                                                            pvCompleteUser);
}

DECLINLINE(int) rawFileFlushAsync(PRAWIMAGE pImage, PVDIOCTX pIoCtx,
                                  PFNVDXFERCOMPLETED pfnComplete,
                                  void *pvCompleteUser)
{
    return pImage->pInterfaceIOCallbacks->pfnFlushAsync(pImage->pInterfaceIO->pvUser,
                                                        pImage->pStorage,
                                                        pIoCtx, pfnComplete,
                                                        pvCompleteUser);
}


/**
 * Internal. Flush image data to disk.
 */
static int rawFlushImage(PRAWIMAGE pImage)
{
    int rc = VINF_SUCCESS;

    if (   pImage->pStorage
        && !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        rc = rawFileFlushSync(pImage);

    return rc;
}

/**
 * Internal. Free all allocated space for representing an image except pImage,
 * and optionally delete the image from disk.
 */
static int rawFreeImage(PRAWIMAGE pImage, bool fDelete)
{
    int rc = VINF_SUCCESS;

    /* Freeing a never allocated image (e.g. because the open failed) is
     * not signalled as an error. After all nothing bad happens. */
    if (pImage)
    {
        if (pImage->pStorage)
        {
            /* No point updating the file that is deleted anyway. */
            if (!fDelete)
            {
                /* For newly created images in sequential mode fill it to
                 * the nominal size. */
                if (   pImage->uOpenFlags & VD_OPEN_FLAGS_SEQUENTIAL
                    && !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
                    && pImage->fCreate)
                {
                    /* Fill rest of image with zeroes, a must for sequential
                     * images to reach the nominal size. */
                    uint64_t uOff;
                    void *pvBuf = RTMemTmpAllocZ(RAW_FILL_SIZE);
                    if (!pvBuf)
                        goto out;

                    uOff = pImage->offAccess;
                    /* Write data to all image blocks. */
                    while (uOff < pImage->cbSize)
                    {
                        unsigned cbChunk = (unsigned)RT_MIN(pImage->cbSize,
                                                            RAW_FILL_SIZE);

                        rc = rawFileWriteSync(pImage, uOff, pvBuf, cbChunk,
                                              NULL);
                        if (RT_FAILURE(rc))
                            goto out;

                        uOff += cbChunk;
                    }
out:
                    if (pvBuf)
                        RTMemTmpFree(pvBuf);
                }
                rawFlushImage(pImage);
            }

            rawFileClose(pImage);
            pImage->pStorage = NULL;
        }

        if (fDelete && pImage->pszFilename)
            rawFileDelete(pImage, pImage->pszFilename);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Internal: Open an image, constructing all necessary data structures.
 */
static int rawOpenImage(PRAWIMAGE pImage, unsigned uOpenFlags)
{
    int rc;

    pImage->uOpenFlags = uOpenFlags;
    pImage->fCreate = false;

    pImage->pInterfaceError = VDInterfaceGet(pImage->pVDIfsDisk, VDINTERFACETYPE_ERROR);
    if (pImage->pInterfaceError)
        pImage->pInterfaceErrorCallbacks = VDGetInterfaceError(pImage->pInterfaceError);

    /* Get I/O interface. */
    pImage->pInterfaceIO = VDInterfaceGet(pImage->pVDIfsImage, VDINTERFACETYPE_IOINT);
    AssertPtrReturn(pImage->pInterfaceIO, VERR_INVALID_PARAMETER);
    pImage->pInterfaceIOCallbacks = VDGetInterfaceIOInt(pImage->pInterfaceIO);
    AssertPtrReturn(pImage->pInterfaceIOCallbacks, VERR_INVALID_PARAMETER);

    /*
     * Open the image.
     */
    rc = rawFileOpen(pImage, pImage->pszFilename,
                     VDOpenFlagsToFileOpenFlags(uOpenFlags,
                                                false /* fCreate */));
    if (RT_FAILURE(rc))
    {
        /* Do NOT signal an appropriate error here, as the VD layer has the
         * choice of retrying the open if it failed. */
        goto out;
    }

    rc = rawFileGetSize(pImage, &pImage->cbSize);
    if (RT_FAILURE(rc))
        goto out;
    if (pImage->cbSize % 512)
    {
        rc = VERR_VD_RAW_INVALID_HEADER;
        goto out;
    }
    pImage->uImageFlags |= VD_IMAGE_FLAGS_FIXED;

out:
    if (RT_FAILURE(rc))
        rawFreeImage(pImage, false);
    return rc;
}

/**
 * Internal: Create a raw image.
 */
static int rawCreateImage(PRAWIMAGE pImage, uint64_t cbSize,
                          unsigned uImageFlags, const char *pszComment,
                          PCVDGEOMETRY pPCHSGeometry,
                          PCVDGEOMETRY pLCHSGeometry, unsigned uOpenFlags,
                          PFNVDPROGRESS pfnProgress, void *pvUser,
                          unsigned uPercentStart, unsigned uPercentSpan)
{
    int rc;
    RTFOFF cbFree = 0;
    uint64_t uOff;
    void *pvBuf = NULL;
    int32_t fOpen;

    if (uImageFlags & VD_IMAGE_FLAGS_DIFF)
    {
        rc = rawError(pImage, VERR_VD_RAW_INVALID_TYPE, RT_SRC_POS, N_("Raw: cannot create diff image '%s'"), pImage->pszFilename);
        goto out;
    }
    uImageFlags |= VD_IMAGE_FLAGS_FIXED;

    pImage->uImageFlags = uImageFlags;

    pImage->uOpenFlags = uOpenFlags & ~VD_OPEN_FLAGS_READONLY;

    pImage->uImageFlags = uImageFlags;
    pImage->fCreate = true;
    pImage->PCHSGeometry = *pPCHSGeometry;
    pImage->LCHSGeometry = *pLCHSGeometry;

    pImage->pInterfaceError = VDInterfaceGet(pImage->pVDIfsDisk, VDINTERFACETYPE_ERROR);
    if (pImage->pInterfaceError)
        pImage->pInterfaceErrorCallbacks = VDGetInterfaceError(pImage->pInterfaceError);

    /* Get I/O interface. */
    pImage->pInterfaceIO = VDInterfaceGet(pImage->pVDIfsImage, VDINTERFACETYPE_IOINT);
    AssertPtrReturn(pImage->pInterfaceIO, VERR_INVALID_PARAMETER);
    pImage->pInterfaceIOCallbacks = VDGetInterfaceIOInt(pImage->pInterfaceIO);
    AssertPtrReturn(pImage->pInterfaceIOCallbacks, VERR_INVALID_PARAMETER);

    /* Create image file. */
    fOpen = VDOpenFlagsToFileOpenFlags(pImage->uOpenFlags, true /* fCreate */);
    if (uOpenFlags & VD_OPEN_FLAGS_SEQUENTIAL)
        fOpen &= ~RTFILE_O_READ;
    rc = rawFileOpen(pImage, pImage->pszFilename, fOpen);
    if (RT_FAILURE(rc))
    {
        rc = rawError(pImage, rc, RT_SRC_POS, N_("Raw: cannot create image '%s'"), pImage->pszFilename);
        goto out;
    }

    if (!(uOpenFlags & VD_OPEN_FLAGS_SEQUENTIAL))
    {
        /* Check the free space on the disk and leave early if there is not
         * sufficient space available. */
        rc = rawFileGetFreeSpace(pImage, pImage->pszFilename, &cbFree);
        if (RT_SUCCESS(rc) /* ignore errors */ && ((uint64_t)cbFree < cbSize))
        {
            rc = rawError(pImage, VERR_DISK_FULL, RT_SRC_POS, N_("Raw: disk would overflow creating image '%s'"), pImage->pszFilename);
            goto out;
        }

        /* Allocate & commit whole file if fixed image, it must be more
         * effective than expanding file by write operations. */
        rc = rawFileSetSize(pImage, cbSize);
        if (RT_FAILURE(rc))
        {
            rc = rawError(pImage, rc, RT_SRC_POS, N_("Raw: setting image size failed for '%s'"), pImage->pszFilename);
            goto out;
        }

        /* Fill image with zeroes. We do this for every fixed-size image since
         * on some systems (for example Windows Vista), it takes ages to write
         * a block near the end of a sparse file and the guest could complain
         * about an ATA timeout. */
        pvBuf = RTMemTmpAllocZ(RAW_FILL_SIZE);
        if (!pvBuf)
        {
            rc = VERR_NO_MEMORY;
            goto out;
        }

        uOff = 0;
        /* Write data to all image blocks. */
        while (uOff < cbSize)
        {
            unsigned cbChunk = (unsigned)RT_MIN(cbSize, RAW_FILL_SIZE);

            rc = rawFileWriteSync(pImage, uOff, pvBuf, cbChunk, NULL);
            if (RT_FAILURE(rc))
            {
                rc = rawError(pImage, rc, RT_SRC_POS, N_("Raw: writing block failed for '%s'"), pImage->pszFilename);
                goto out;
            }

            uOff += cbChunk;

            if (pfnProgress)
            {
                rc = pfnProgress(pvUser,
                                 uPercentStart + uOff * uPercentSpan * 98 / (cbSize * 100));
                if (RT_FAILURE(rc))
                    goto out;
            }
        }
    }

    if (RT_SUCCESS(rc) && pfnProgress)
        pfnProgress(pvUser, uPercentStart + uPercentSpan * 98 / 100);

    pImage->cbSize = cbSize;

    rc = rawFlushImage(pImage);

out:
    if (pvBuf)
        RTMemTmpFree(pvBuf);

    if (RT_SUCCESS(rc) && pfnProgress)
        pfnProgress(pvUser, uPercentStart + uPercentSpan);

    if (RT_FAILURE(rc))
        rawFreeImage(pImage, rc != VERR_ALREADY_EXISTS);
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnCheckIfValid */
static int rawCheckIfValid(const char *pszFilename, PVDINTERFACE pVDIfsDisk,
                           PVDINTERFACE pVDIfsImage, VDTYPE *penmType)
{
    LogFlowFunc(("pszFilename=\"%s\" pVDIfsDisk=%#p pVDIfsImage=%#p\n", pszFilename, pVDIfsDisk, pVDIfsImage));
    PVDIOSTORAGE pStorage = NULL;
    uint64_t cbFile;
    int rc = VINF_SUCCESS;
    ISOVOLDESC IsoVolDesc;
    char *pszExtension = NULL;

    /* Get I/O interface. */
    PVDINTERFACE pInterfaceIO = VDInterfaceGet(pVDIfsImage, VDINTERFACETYPE_IOINT);
    AssertPtrReturn(pInterfaceIO, VERR_INVALID_PARAMETER);
    PVDINTERFACEIOINT pInterfaceIOCallbacks = VDGetInterfaceIOInt(pInterfaceIO);
    AssertPtrReturn(pInterfaceIOCallbacks, VERR_INVALID_PARAMETER);

    if (   !VALID_PTR(pszFilename)
        || !*pszFilename)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    pszExtension = RTPathExt(pszFilename);

    /*
     * Open the file and read the footer.
     */
    rc = pInterfaceIOCallbacks->pfnOpen(pInterfaceIO->pvUser, pszFilename,
                                        VDOpenFlagsToFileOpenFlags(VD_OPEN_FLAGS_READONLY,
                                                                   false /* fCreate */),
                                        &pStorage);
    if (RT_SUCCESS(rc))
        rc = pInterfaceIOCallbacks->pfnGetSize(pInterfaceIO->pvUser, pStorage,
                                               &cbFile);

    /* Try to guess the image type based on the extension. */
    if (   RT_SUCCESS(rc)
        && pszExtension)
    {
        if (!strcmp(pszExtension, ".iso")) /* DVD images. */
        {
            if (cbFile >= (32768 + sizeof(ISOVOLDESC)))
            {
                rc = pInterfaceIOCallbacks->pfnReadSync(pInterfaceIO->pvUser, pStorage,
                                                        32768, &IsoVolDesc, sizeof(IsoVolDesc), NULL);
            }
            else
                rc = VERR_VD_RAW_INVALID_HEADER;

            if (RT_SUCCESS(rc))
            {
                /*
                 * Do we recognize this stuff?
                 */
                if (   !memcmp(IsoVolDesc.fields.au8Id, "BEA01", 5)
                    || !memcmp(IsoVolDesc.fields.au8Id, "BOOT2", 5)
                    || !memcmp(IsoVolDesc.fields.au8Id, "CD001", 5)
                    || !memcmp(IsoVolDesc.fields.au8Id, "CDW02", 5)
                    || !memcmp(IsoVolDesc.fields.au8Id, "NSR02", 5)
                    || !memcmp(IsoVolDesc.fields.au8Id, "NSR03", 5)
                    || !memcmp(IsoVolDesc.fields.au8Id, "TEA01", 5))
                {
                    *penmType = VDTYPE_DVD;
                    rc = VINF_SUCCESS;
                }
                else
                    rc = VERR_VD_RAW_INVALID_HEADER;
            }
        }
        else if (!strcmp(pszExtension, ".img") || !strcmp(pszExtension, ".ima")) /* Floppy images */
        {
            if (!(cbFile % 512))
            {
                *penmType = VDTYPE_FLOPPY;
                rc = VINF_SUCCESS;
            }
            else
                rc = VERR_VD_RAW_INVALID_HEADER;
        }
    }
    else
        rc = VERR_VD_RAW_INVALID_HEADER;

    if (pStorage)
        pInterfaceIOCallbacks->pfnClose(pInterfaceIO->pvUser, pStorage);

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnOpen */
static int rawOpen(const char *pszFilename, unsigned uOpenFlags,
                   PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                   VDTYPE enmType, void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" uOpenFlags=%#x pVDIfsDisk=%#p pVDIfsImage=%#p ppBackendData=%#p\n", pszFilename, uOpenFlags, pVDIfsDisk, pVDIfsImage, ppBackendData));
    int rc;
    PRAWIMAGE pImage;

    /* Check open flags. All valid flags are supported. */
    if (uOpenFlags & ~VD_OPEN_FLAGS_MASK)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Check remaining arguments. */
    if (   !VALID_PTR(pszFilename)
        || !*pszFilename)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }


    pImage = (PRAWIMAGE)RTMemAllocZ(sizeof(RAWIMAGE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pImage->pszFilename = pszFilename;
    pImage->pStorage = NULL;
    pImage->pVDIfsDisk = pVDIfsDisk;
    pImage->pVDIfsImage = pVDIfsImage;

    rc = rawOpenImage(pImage, uOpenFlags);
    if (RT_SUCCESS(rc))
        *ppBackendData = pImage;
    else
        RTMemFree(pImage);

out:
    LogFlowFunc(("returns %Rrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnCreate */
static int rawCreate(const char *pszFilename, uint64_t cbSize,
                     unsigned uImageFlags, const char *pszComment,
                     PCVDGEOMETRY pPCHSGeometry, PCVDGEOMETRY pLCHSGeometry,
                     PCRTUUID pUuid, unsigned uOpenFlags,
                     unsigned uPercentStart, unsigned uPercentSpan,
                     PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                     PVDINTERFACE pVDIfsOperation, void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" cbSize=%llu uImageFlags=%#x pszComment=\"%s\" pPCHSGeometry=%#p pLCHSGeometry=%#p Uuid=%RTuuid uOpenFlags=%#x uPercentStart=%u uPercentSpan=%u pVDIfsDisk=%#p pVDIfsImage=%#p pVDIfsOperation=%#p ppBackendData=%#p", pszFilename, cbSize, uImageFlags, pszComment, pPCHSGeometry, pLCHSGeometry, pUuid, uOpenFlags, uPercentStart, uPercentSpan, pVDIfsDisk, pVDIfsImage, pVDIfsOperation, ppBackendData));
    int rc;
    PRAWIMAGE pImage;

    PFNVDPROGRESS pfnProgress = NULL;
    void *pvUser = NULL;
    PVDINTERFACE pIfProgress = VDInterfaceGet(pVDIfsOperation,
                                              VDINTERFACETYPE_PROGRESS);
    PVDINTERFACEPROGRESS pCbProgress = NULL;
    if (pIfProgress)
    {
        pCbProgress = VDGetInterfaceProgress(pIfProgress);
        if (pCbProgress)
            pfnProgress = pCbProgress->pfnProgress;
        pvUser = pIfProgress->pvUser;
    }

    /* Check open flags. All valid flags are supported. */
    if (uOpenFlags & ~VD_OPEN_FLAGS_MASK)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Check remaining arguments. */
    if (   !VALID_PTR(pszFilename)
        || !*pszFilename
        || !VALID_PTR(pPCHSGeometry)
        || !VALID_PTR(pLCHSGeometry))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    pImage = (PRAWIMAGE)RTMemAllocZ(sizeof(RAWIMAGE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pImage->pszFilename = pszFilename;
    pImage->pStorage = NULL;
    pImage->pVDIfsDisk = pVDIfsDisk;
    pImage->pVDIfsImage = pVDIfsImage;

    rc = rawCreateImage(pImage, cbSize, uImageFlags, pszComment,
                        pPCHSGeometry, pLCHSGeometry, uOpenFlags,
                        pfnProgress, pvUser, uPercentStart, uPercentSpan);
    if (RT_SUCCESS(rc))
    {
        /* So far the image is opened in read/write mode. Make sure the
         * image is opened in read-only mode if the caller requested that. */
        if (uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            rawFreeImage(pImage, false);
            rc = rawOpenImage(pImage, uOpenFlags);
            if (RT_FAILURE(rc))
            {
                RTMemFree(pImage);
                goto out;
            }
        }
        *ppBackendData = pImage;
    }
    else
        RTMemFree(pImage);

out:
    LogFlowFunc(("returns %Rrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnRename */
static int rawRename(void *pBackendData, const char *pszFilename)
{
    LogFlowFunc(("pBackendData=%#p pszFilename=%#p\n", pBackendData, pszFilename));
    int rc = VINF_SUCCESS;
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;

    /* Check arguments. */
    if (   !pImage
        || !pszFilename
        || !*pszFilename)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Close the image. */
    rc = rawFreeImage(pImage, false);
    if (RT_FAILURE(rc))
        goto out;

    /* Rename the file. */
    rc = rawFileMove(pImage, pImage->pszFilename, pszFilename, 0);
    if (RT_FAILURE(rc))
    {
        /* The move failed, try to reopen the original image. */
        int rc2 = rawOpenImage(pImage, pImage->uOpenFlags);
        if (RT_FAILURE(rc2))
            rc = rc2;

        goto out;
    }

    /* Update pImage with the new information. */
    pImage->pszFilename = pszFilename;

    /* Open the old image with new name. */
    rc = rawOpenImage(pImage, pImage->uOpenFlags);
    if (RT_FAILURE(rc))
        goto out;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnClose */
static int rawClose(void *pBackendData, bool fDelete)
{
    LogFlowFunc(("pBackendData=%#p fDelete=%d\n", pBackendData, fDelete));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    rc = rawFreeImage(pImage, fDelete);
    RTMemFree(pImage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnRead */
static int rawRead(void *pBackendData, uint64_t uOffset, void *pvBuf,
                   size_t cbToRead, size_t *pcbActuallyRead)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToRead=%zu pcbActuallyRead=%#p\n", pBackendData, uOffset, pvBuf, cbToRead, pcbActuallyRead));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToRead % 512 == 0);

    if (   uOffset + cbToRead > pImage->cbSize
        || cbToRead == 0)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* For sequential access do not allow to go back. */
    if (   pImage->uOpenFlags & VD_OPEN_FLAGS_SEQUENTIAL
        && uOffset < pImage->offAccess)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    rc = rawFileReadSync(pImage, uOffset, pvBuf, cbToRead, NULL);
    pImage->offAccess = uOffset + cbToRead;
    if (pcbActuallyRead)
        *pcbActuallyRead = cbToRead;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnWrite */
static int rawWrite(void *pBackendData, uint64_t uOffset, const void *pvBuf,
                    size_t cbToWrite, size_t *pcbWriteProcess,
                    size_t *pcbPreRead, size_t *pcbPostRead, unsigned fWrite)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToWrite=%zu pcbWriteProcess=%#p pcbPreRead=%#p pcbPostRead=%#p\n", pBackendData, uOffset, pvBuf, cbToWrite, pcbWriteProcess, pcbPreRead, pcbPostRead));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToWrite % 512 == 0);

    if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
    {
        rc = VERR_VD_IMAGE_READ_ONLY;
        goto out;
    }

    if (   uOffset + cbToWrite > pImage->cbSize
        || cbToWrite == 0)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* For sequential access do not allow to go back. */
    if (   pImage->uOpenFlags & VD_OPEN_FLAGS_SEQUENTIAL
        && uOffset < pImage->offAccess)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    rc = rawFileWriteSync(pImage, uOffset, pvBuf, cbToWrite, NULL);
    pImage->offAccess = uOffset + cbToWrite;
    if (pcbWriteProcess)
        *pcbWriteProcess = cbToWrite;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnFlush */
static int rawFlush(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    rc = rawFlushImage(pImage);
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetVersion */
static unsigned rawGetVersion(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;

    AssertPtr(pImage);

    if (pImage)
        return 1;
    else
        return 0;
}

/** @copydoc VBOXHDDBACKEND::pfnGetSize */
static uint64_t rawGetSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    uint64_t cb = 0;

    AssertPtr(pImage);

    if (pImage && pImage->pStorage)
        cb = pImage->cbSize;

    LogFlowFunc(("returns %llu\n", cb));
    return cb;
}

/** @copydoc VBOXHDDBACKEND::pfnGetFileSize */
static uint64_t rawGetFileSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    uint64_t cb = 0;

    AssertPtr(pImage);

    if (pImage)
    {
        uint64_t cbFile;
        if (pImage->pStorage)
        {
            int rc = rawFileGetSize(pImage, &cbFile);
            if (RT_SUCCESS(rc))
                cb += cbFile;
        }
    }

    LogFlowFunc(("returns %lld\n", cb));
    return cb;
}

/** @copydoc VBOXHDDBACKEND::pfnGetPCHSGeometry */
static int rawGetPCHSGeometry(void *pBackendData,
                              PVDGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p\n", pBackendData, pPCHSGeometry));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (pImage->PCHSGeometry.cCylinders)
        {
            *pPCHSGeometry = pImage->PCHSGeometry;
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_VD_GEOMETRY_NOT_SET;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (PCHS=%u/%u/%u)\n", rc, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetPCHSGeometry */
static int rawSetPCHSGeometry(void *pBackendData,
                              PCVDGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p PCHS=%u/%u/%u\n", pBackendData, pPCHSGeometry, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            rc = VERR_VD_IMAGE_READ_ONLY;
            goto out;
        }

        pImage->PCHSGeometry = *pPCHSGeometry;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VD_NOT_OPENED;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetLCHSGeometry */
static int rawGetLCHSGeometry(void *pBackendData,
                              PVDGEOMETRY pLCHSGeometry)
{
     LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p\n", pBackendData, pLCHSGeometry));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (pImage->LCHSGeometry.cCylinders)
        {
            *pLCHSGeometry = pImage->LCHSGeometry;
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_VD_GEOMETRY_NOT_SET;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (LCHS=%u/%u/%u)\n", rc, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetLCHSGeometry */
static int rawSetLCHSGeometry(void *pBackendData,
                               PCVDGEOMETRY pLCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p LCHS=%u/%u/%u\n", pBackendData, pLCHSGeometry, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            rc = VERR_VD_IMAGE_READ_ONLY;
            goto out;
        }

        pImage->LCHSGeometry = *pLCHSGeometry;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VD_NOT_OPENED;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetImageFlags */
static unsigned rawGetImageFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    unsigned uImageFlags;

    AssertPtr(pImage);

    if (pImage)
        uImageFlags = pImage->uImageFlags;
    else
        uImageFlags = 0;

    LogFlowFunc(("returns %#x\n", uImageFlags));
    return uImageFlags;
}

/** @copydoc VBOXHDDBACKEND::pfnGetOpenFlags */
static unsigned rawGetOpenFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    unsigned uOpenFlags;

    AssertPtr(pImage);

    if (pImage)
        uOpenFlags = pImage->uOpenFlags;
    else
        uOpenFlags = 0;

    LogFlowFunc(("returns %#x\n", uOpenFlags));
    return uOpenFlags;
}

/** @copydoc VBOXHDDBACKEND::pfnSetOpenFlags */
static int rawSetOpenFlags(void *pBackendData, unsigned uOpenFlags)
{
    LogFlowFunc(("pBackendData=%#p\n uOpenFlags=%#x", pBackendData, uOpenFlags));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    /* Image must be opened and the new flags must be valid. */
    if (!pImage || (uOpenFlags & ~(VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO | VD_OPEN_FLAGS_ASYNC_IO | VD_OPEN_FLAGS_SHAREABLE | VD_OPEN_FLAGS_SEQUENTIAL)))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Implement this operation via reopening the image. */
    rc = rawFreeImage(pImage, false);
    if (RT_FAILURE(rc))
        goto out;
    rc = rawOpenImage(pImage, uOpenFlags);

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetComment */
static int rawGetComment(void *pBackendData, char *pszComment,
                          size_t cbComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=%#p cbComment=%zu\n", pBackendData, pszComment, cbComment));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc comment='%s'\n", rc, pszComment));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetComment */
static int rawSetComment(void *pBackendData, const char *pszComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=\"%s\"\n", pBackendData, pszComment));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
            rc = VERR_VD_IMAGE_READ_ONLY;
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        rc = VERR_VD_NOT_OPENED;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetUuid */
static int rawGetUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetUuid */
static int rawSetUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    LogFlowFunc(("%RTuuid\n", pUuid));
    AssertPtr(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VD_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetModificationUuid */
static int rawGetModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetModificationUuid */
static int rawSetModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VD_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetParentUuid */
static int rawGetParentUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentUuid */
static int rawSetParentUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VD_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetParentModificationUuid */
static int rawGetParentModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentModificationUuid */
static int rawSetParentModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VD_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnDump */
static void rawDump(void *pBackendData)
{
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;

    AssertPtr(pImage);
    if (pImage)
    {
        rawMessage(pImage, "Header: Geometry PCHS=%u/%u/%u LCHS=%u/%u/%u cbSector=%llu\n",
                    pImage->PCHSGeometry.cCylinders, pImage->PCHSGeometry.cHeads, pImage->PCHSGeometry.cSectors,
                    pImage->LCHSGeometry.cCylinders, pImage->LCHSGeometry.cHeads, pImage->LCHSGeometry.cSectors,
                    pImage->cbSize / 512);
    }
}

/** @copydoc VBOXHDDBACKEND::pfnIsAsyncIOSupported */
static bool rawIsAsyncIOSupported(void *pBackendData)
{
    return true;
}

/** @copydoc VBOXHDDBACKEND::pfnAsyncRead */
static int rawAsyncRead(void *pBackendData, uint64_t uOffset, size_t cbRead,
                        PVDIOCTX pIoCtx, size_t *pcbActuallyRead)
{
    int rc = VINF_SUCCESS;
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;

    rc = rawFileReadUserAsync(pImage, uOffset, pIoCtx, cbRead);
    if (RT_SUCCESS(rc))
        *pcbActuallyRead = cbRead;

    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnAsyncWrite */
static int rawAsyncWrite(void *pBackendData, uint64_t uOffset, size_t cbWrite,
                         PVDIOCTX pIoCtx,
                         size_t *pcbWriteProcess, size_t *pcbPreRead,
                         size_t *pcbPostRead, unsigned fWrite)
{
    int rc = VINF_SUCCESS;
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;

    rc = rawFileWriteUserAsync(pImage, uOffset, pIoCtx, cbWrite, NULL, NULL);
    if (RT_SUCCESS(rc))
    {
        *pcbWriteProcess = cbWrite;
        *pcbPostRead = 0;
        *pcbPreRead  = 0;
    }

    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnAsyncFlush */
static int rawAsyncFlush(void *pBackendData, PVDIOCTX pIoCtx)
{
    int rc = VINF_SUCCESS;
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;

    if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        rc = rawFileFlushAsync(pImage, pIoCtx, NULL, NULL);

    return rc;
}


VBOXHDDBACKEND g_RawBackend =
{
    /* pszBackendName */
    "RAW",
    /* cbSize */
    sizeof(VBOXHDDBACKEND),
    /* uBackendCaps */
    VD_CAP_CREATE_FIXED | VD_CAP_FILE | VD_CAP_ASYNC | VD_CAP_VFS,
    /* paFileExtensions */
    s_aRawFileExtensions,
    /* paConfigInfo */
    NULL,
    /* hPlugin */
    NIL_RTLDRMOD,
    /* pfnCheckIfValid */
    rawCheckIfValid,
    /* pfnOpen */
    rawOpen,
    /* pfnCreate */
    rawCreate,
    /* pfnRename */
    rawRename,
    /* pfnClose */
    rawClose,
    /* pfnRead */
    rawRead,
    /* pfnWrite */
    rawWrite,
    /* pfnFlush */
    rawFlush,
    /* pfnGetVersion */
    rawGetVersion,
    /* pfnGetSize */
    rawGetSize,
    /* pfnGetFileSize */
    rawGetFileSize,
    /* pfnGetPCHSGeometry */
    rawGetPCHSGeometry,
    /* pfnSetPCHSGeometry */
    rawSetPCHSGeometry,
    /* pfnGetLCHSGeometry */
    rawGetLCHSGeometry,
    /* pfnSetLCHSGeometry */
    rawSetLCHSGeometry,
    /* pfnGetImageFlags */
    rawGetImageFlags,
    /* pfnGetOpenFlags */
    rawGetOpenFlags,
    /* pfnSetOpenFlags */
    rawSetOpenFlags,
    /* pfnGetComment */
    rawGetComment,
    /* pfnSetComment */
    rawSetComment,
    /* pfnGetUuid */
    rawGetUuid,
    /* pfnSetUuid */
    rawSetUuid,
    /* pfnGetModificationUuid */
    rawGetModificationUuid,
    /* pfnSetModificationUuid */
    rawSetModificationUuid,
    /* pfnGetParentUuid */
    rawGetParentUuid,
    /* pfnSetParentUuid */
    rawSetParentUuid,
    /* pfnGetParentModificationUuid */
    rawGetParentModificationUuid,
    /* pfnSetParentModificationUuid */
    rawSetParentModificationUuid,
    /* pfnDump */
    rawDump,
    /* pfnGetTimeStamp */
    NULL,
    /* pfnGetParentTimeStamp */
    NULL,
    /* pfnSetParentTimeStamp */
    NULL,
    /* pfnGetParentFilename */
    NULL,
    /* pfnSetParentFilename */
    NULL,
    /* pfnIsAsyncIOSupported */
    rawIsAsyncIOSupported,
    /* pfnAsyncRead */
    rawAsyncRead,
    /* pfnAsyncWrite */
    rawAsyncWrite,
    /* pfnAsyncFlush */
    rawAsyncFlush,
    /* pfnComposeLocation */
    genericFileComposeLocation,
    /* pfnComposeName */
    genericFileComposeName,
    /* pfnCompact */
    NULL,
    /* pfnResize */
    NULL
};
