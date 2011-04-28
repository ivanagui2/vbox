/* $Id$ */
/** @file
 * IPRT Disk Volume Management API (DVM) - GPT format backend.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#include <iprt/types.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/dvm.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include "internal/dvm.h"

/** The GPT signature. */
#define RTDVM_GPT_SIGNATURE "EFI PART"

/**
 * GPT on disk header.
 */
#pragma pack(1)
typedef struct GptHdr
{
    /** Signature ("EFI PART"). */
    char     abSignature[8];
    /** Revision. */
    uint32_t u32Revision;
    /** Header size. */
    uint32_t cbHeader;
    /** CRC of header. */
    uint32_t u32Crc;
} GptHdr;
/** Pointer to a GPT header. */
typedef struct GptHdr *PGptHdr;
#pragma pack()
AssertCompileSize(GptHdr, 20);

/**
 * Complete GPT table header for revision 1.0.
 */
#pragma pack(1)
typedef struct GptHdrRev1
{
    /** Header. */
    GptHdr   Hdr;
    /** Reserved. */
    uint32_t u32Reserved;
    /** Current LBA. */
    uint64_t u64LbaCurrent;
    /** Backup LBA. */
    uint64_t u64LbaBackup;
    /** First usable LBA for partitions. */
    uint64_t u64LbaFirstPartition;
    /** Last usable LBA for partitions. */
    uint64_t u64LbaLastPartition;
    /** Disk UUID. */
    RTUUID   DiskUuid;
    /** LBA of first partition entry. */
    uint64_t u64LbaPartitionEntries;
    /** Number of partition entries. */
    uint32_t cPartitionEntries;
    /** Partition entry size. */
    uint32_t cbPartitionEntry;
    /** CRC of partition entries. */
    uint32_t u32CrcPartitionEntries;
} GptHdrRev1;
/** Pointer to a revision 1.0 GPT header. */
typedef GptHdrRev1 *PGptHdrRev1;
#pragma pack()
AssertCompileSize(GptHdrRev1, 92);

/**
 * GPT partition table entry.
 */
#pragma pack(1)
typedef struct GptEntry
{
    /** Partition type UUID. */
    RTUUID   UuidType;
    /** Partition UUID. */
    RTUUID   UuidPartition;
    /** First LBA. */
    uint64_t u64LbaFirst;
    /** Last LBA. */
    uint64_t u64LbaLast;
    /** Attribute flags. */
    uint64_t u64Flags;
    /** Partition name (UTF-16LE code units). */
    RTUTF16  aPartitionName[36];
} GptEntry;
/** Pointer to a GPT entry. */
typedef struct GptEntry *PGptEntry;
#pragma pack()
AssertCompileSize(GptEntry, 128);

/** Partition flags - System partition. */
#define RTDVM_GPT_ENTRY_SYSTEM          RT_BIT_64(0)
/** Partition flags - Partition is readonly. */
#define RTDVM_GPT_ENTRY_READONLY        RT_BIT_64(60)
/** Partition flags - Partition is hidden. */
#define RTDVM_GPT_ENTRY_HIDDEN          RT_BIT_64(62)
/** Partition flags - Don't automount this partition. */
#define RTDVM_GPT_ENTRY_NO_AUTOMOUNT    RT_BIT_64(63)

/**
 * GPT volume manager data.
 */
typedef struct RTDVMFMTINTERNAL
{
    /** Pointer to the underlying disk. */
    PCRTDVMDISK     pDisk;
    /** GPT header. */
    GptHdrRev1      HdrRev1;
    /** GPT array. */
    PGptEntry       paGptEntries;
    /** Number of occupied partition entries. */
    uint32_t        cPartitions;
} RTDVMFMTINTERNAL;
/** Pointer to the MBR volume manager. */
typedef RTDVMFMTINTERNAL *PRTDVMFMTINTERNAL;

/**
 * GPT volume data.
 */
typedef struct RTDVMVOLUMEFMTINTERNAL
{
    /** Pointer to the volume manager. */
    PRTDVMFMTINTERNAL pVolMgr;
    /** Partition table entry index. */
    uint32_t          idxEntry;
    /** Start offset of the volume. */
    uint64_t          offStart;
    /** Size of the volume. */
    uint64_t          cbVolume;
    /** Pointer to the GPT entry in the array. */
    PGptEntry         pGptEntry;
} RTDVMVOLUMEFMTINTERNAL;
/** Pointer to an MBR volume. */
typedef RTDVMVOLUMEFMTINTERNAL *PRTDVMVOLUMEFMTINTERNAL;

/**
 * GPT partition type to DVM volume type mapping entry.
 */

typedef struct RTDVMGPTPARTTYPE2VOLTYPE
{
    /** Type UUID. */
    const char       *pcszUuid;
    /** DVM volume type. */
    RTDVMVOLTYPE      enmVolType;
} RTDVMGPTPARTTYPE2VOLTYPE;
/** Pointer to a MBR FS Type to volume type mapping entry. */
typedef RTDVMGPTPARTTYPE2VOLTYPE *PRTDVMGPTPARTTYPE2VOLTYPE;

/** Converts a LBA number to the byte offset. */
#define RTDVM_GPT_LBA2BYTE(lba, disk) ((lba) * (disk)->cbSector)
/** Converts a Byte offset to the LBA number. */
#define RTDVM_GPT_BYTE2LBA(lba, disk) ((lba) / (disk)->cbSector)

/**
 * Mapping of partition types to DVM volume types.
 *
 * From http://en.wikipedia.org/wiki/GUID_Partition_Table
 */
static const RTDVMGPTPARTTYPE2VOLTYPE g_aPartType2DvmVolTypes[] =
{
    {"0657FD6D-A4AB-43C4-84E5-0933C84B4F4F", RTDVMVOLTYPE_LINUX_SWAP},
    {"EBD0A0A2-B9E5-4433-87C0-68B6B72699C7", RTDVMVOLTYPE_LINUX_NATIVE},
    {"E6D6D379-F507-44C2-A23C-238F2A3DF928", RTDVMVOLTYPE_LINUX_LVM},
    {"A19D880F-05FC-4D3B-A006-743F0F84911E", RTDVMVOLTYPE_LINUX_SOFTRAID},

    {"83BD6B9D-7F41-11DC-BE0B-001560B84F0F", RTDVMVOLTYPE_FREEBSD}, /* Boot */
    {"516E7CB4-6ECF-11D6-8FF8-00022D09712B", RTDVMVOLTYPE_FREEBSD}, /* Data */
    {"516E7CB5-6ECF-11D6-8FF8-00022D09712B", RTDVMVOLTYPE_FREEBSD}, /* Swap */
    {"516E7CB6-6ECF-11D6-8FF8-00022D09712B", RTDVMVOLTYPE_FREEBSD}, /* UFS */
    {"516E7CB8-6ECF-11D6-8FF8-00022D09712B", RTDVMVOLTYPE_FREEBSD}, /* Vinum */
    {"516E7CBA-6ECF-11D6-8FF8-00022D09712B", RTDVMVOLTYPE_FREEBSD}, /* ZFS */

    {"49F48D32-B10E-11DC-B99B-0019D1879648", RTDVMVOLTYPE_NETBSD}, /* Swap */
    {"49F48D5A-B10E-11DC-B99B-0019D1879648", RTDVMVOLTYPE_NETBSD}, /* FFS */
    {"49F48D82-B10E-11DC-B99B-0019D1879648", RTDVMVOLTYPE_NETBSD}, /* LFS */
    {"49F48DAA-B10E-11DC-B99B-0019D1879648", RTDVMVOLTYPE_NETBSD}, /* Raid */
    {"2DB519C4-B10F-11DC-B99B-0019D1879648", RTDVMVOLTYPE_NETBSD}, /* Concatenated */
    {"2DB519EC-B10F-11DC-B99B-0019D1879648", RTDVMVOLTYPE_NETBSD}, /* Encrypted */

    {"48465300-0000-11AA-AA11-00306543ECAC", RTDVMVOLTYPE_MAC_OSX_HFS},

    {"6A82CB45-1DD2-11B2-99A6-080020736631", RTDVMVOLTYPE_SOLARIS}, /* Boot */
    {"6A85CF4D-1DD2-11B2-99A6-080020736631", RTDVMVOLTYPE_SOLARIS}, /* Root */
    {"6A87C46F-1DD2-11B2-99A6-080020736631", RTDVMVOLTYPE_SOLARIS}, /* Swap */
    {"6A8B642B-1DD2-11B2-99A6-080020736631", RTDVMVOLTYPE_SOLARIS}, /* Backup */
    {"6A898CC3-1DD2-11B2-99A6-080020736631", RTDVMVOLTYPE_SOLARIS}, /* /usr */
    {"6A8EF2E9-1DD2-11B2-99A6-080020736631", RTDVMVOLTYPE_SOLARIS}, /* /var */
    {"6A90BA39-1DD2-11B2-99A6-080020736631", RTDVMVOLTYPE_SOLARIS}, /* /home */
    {"6A9283A5-1DD2-11B2-99A6-080020736631", RTDVMVOLTYPE_SOLARIS}, /* Alternate sector */
};

DECLCALLBACK(int) dvmFmtGptProbe(PCRTDVMDISK pDisk, uint32_t *puScore)
{
    int rc = VINF_SUCCESS;
    GptHdr Hdr;

    *puScore = RTDVM_MATCH_SCORE_UNSUPPORTED;

    if (dvmDiskGetSectors(pDisk) >= 2)
    {
        /* Read from the disk and check for the signature. */
        rc = dvmDiskRead(pDisk, RTDVM_GPT_LBA2BYTE(1, pDisk), &Hdr, sizeof(GptHdr));
        if (   RT_SUCCESS(rc)
            && !strncmp(&Hdr.abSignature[0], RTDVM_GPT_SIGNATURE, RT_ELEMENTS(Hdr.abSignature))
            && RT_LE2H_U32(Hdr.u32Revision) == 0x00010000
            && RT_LE2H_U32(Hdr.cbHeader)    == sizeof(GptHdrRev1))
            *puScore = RTDVM_MATCH_SCORE_PERFECT;
    }

    return rc;
}

DECLCALLBACK(int) dvmFmtGptOpen(PCRTDVMDISK pDisk, PRTDVMFMT phVolMgrFmt)
{
    int rc = VINF_SUCCESS;
    PRTDVMFMTINTERNAL pThis = NULL;

    pThis = (PRTDVMFMTINTERNAL)RTMemAllocZ(sizeof(RTDVMFMTINTERNAL));
    if (VALID_PTR(pThis))
    {
        pThis->pDisk       = pDisk;
        pThis->cPartitions = 0;

        /* Read the complete GPT header and convert to host endianess. */
        rc = dvmDiskRead(pDisk, RTDVM_GPT_LBA2BYTE(1, pDisk), &pThis->HdrRev1, sizeof(pThis->HdrRev1));
        if (RT_SUCCESS(rc))
        {
            pThis->HdrRev1.Hdr.u32Revision        = RT_LE2H_U32(pThis->HdrRev1.Hdr.u32Revision);
            pThis->HdrRev1.Hdr.cbHeader           = RT_LE2H_U32(pThis->HdrRev1.Hdr.cbHeader);
            pThis->HdrRev1.Hdr.u32Crc             = RT_LE2H_U32(pThis->HdrRev1.Hdr.u32Crc);
            pThis->HdrRev1.u64LbaCurrent          = RT_LE2H_U64(pThis->HdrRev1.u64LbaCurrent);
            pThis->HdrRev1.u64LbaBackup           = RT_LE2H_U64(pThis->HdrRev1.u64LbaBackup);
            pThis->HdrRev1.u64LbaFirstPartition   = RT_LE2H_U64(pThis->HdrRev1.u64LbaFirstPartition);
            pThis->HdrRev1.u64LbaLastPartition    = RT_LE2H_U64(pThis->HdrRev1.u64LbaLastPartition);
            /** @todo: Disk UUID */
            pThis->HdrRev1.u64LbaPartitionEntries = RT_LE2H_U64(pThis->HdrRev1.u64LbaPartitionEntries);
            pThis->HdrRev1.cPartitionEntries      = RT_LE2H_U32(pThis->HdrRev1.cPartitionEntries);
            pThis->HdrRev1.cbPartitionEntry       = RT_LE2H_U32(pThis->HdrRev1.cbPartitionEntry);
            pThis->HdrRev1.u32CrcPartitionEntries = RT_LE2H_U32(pThis->HdrRev1.u32CrcPartitionEntries);

            if (pThis->HdrRev1.cbPartitionEntry == sizeof(GptEntry))
            {
                pThis->paGptEntries = (PGptEntry)RTMemAllocZ(pThis->HdrRev1.cPartitionEntries * pThis->HdrRev1.cbPartitionEntry);
                if (VALID_PTR(pThis->paGptEntries))
                {
                    rc = dvmDiskRead(pDisk, RTDVM_GPT_LBA2BYTE(pThis->HdrRev1.u64LbaPartitionEntries, pDisk),
                                     pThis->paGptEntries, pThis->HdrRev1.cPartitionEntries * pThis->HdrRev1.cbPartitionEntry);
                    if (RT_SUCCESS(rc))
                    {
                        /* Count the occupied entries. */
                        for (unsigned i = 0; i < pThis->HdrRev1.cPartitionEntries; i++)
                            if (!RTUuidIsNull(&pThis->paGptEntries[i].UuidType))
                            {
                                /* Convert to host endianess. */
                                /** @todo: Uuids */
                                pThis->paGptEntries[i].u64LbaFirst = RT_LE2H_U64(pThis->paGptEntries[i].u64LbaFirst);
                                pThis->paGptEntries[i].u64LbaLast  = RT_LE2H_U64(pThis->paGptEntries[i].u64LbaLast);
                                pThis->paGptEntries[i].u64Flags    = RT_LE2H_U64(pThis->paGptEntries[i].u64Flags);
                                for (unsigned cwc = 0; cwc < RT_ELEMENTS(pThis->paGptEntries[i].aPartitionName); cwc++)
                                    pThis->paGptEntries[i].aPartitionName[cwc] = RT_LE2H_U16(pThis->paGptEntries[i].aPartitionName[cwc]);

                                pThis->cPartitions++;
                            }
                    }

                    if (RT_FAILURE(rc))
                        RTMemFree(pThis->paGptEntries);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            else
                rc = VERR_NOT_SUPPORTED;

            if (RT_SUCCESS(rc))
                *phVolMgrFmt = pThis;
            else
                RTMemFree(pThis);
        }
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

DECLCALLBACK(int) dvmFmtGptInitialize(PCRTDVMDISK pDisk, PRTDVMFMT phVolMgrFmt)
{
    return VERR_NOT_IMPLEMENTED;
}

DECLCALLBACK(void) dvmFmtGptClose(RTDVMFMT hVolMgrFmt)
{
    PRTDVMFMTINTERNAL pThis = hVolMgrFmt;

    pThis->pDisk = NULL;
    memset(&pThis->HdrRev1, 0, sizeof(pThis->HdrRev1));
    RTMemFree(pThis->paGptEntries);

    pThis->paGptEntries = NULL;
    RTMemFree(pThis);
}

DECLCALLBACK(uint32_t) dvmFmtGptGetValidVolumes(RTDVMFMT hVolMgrFmt)
{
    PRTDVMFMTINTERNAL pThis = hVolMgrFmt;

    return pThis->cPartitions;
}

DECLCALLBACK(uint32_t) dvmFmtGptGetMaxVolumes(RTDVMFMT hVolMgrFmt)
{
    PRTDVMFMTINTERNAL pThis = hVolMgrFmt;

    return pThis->HdrRev1.cPartitionEntries;
}

/**
 * Creates a new volume.
 *
 * @returns IPRT status code.
 * @param   pThis         The MBR volume manager data.
 * @param   pGptEntry     The GPT entry.
 * @param   idx           The index in the partition array.
 * @param   phVolFmt      Where to store the volume data on success.
 */
static int dvmFmtMbrVolumeCreate(PRTDVMFMTINTERNAL pThis, PGptEntry pGptEntry,
                                 uint32_t idx, PRTDVMVOLUMEFMT phVolFmt)
{
    int rc = VINF_SUCCESS;
    PRTDVMVOLUMEFMTINTERNAL pVol = (PRTDVMVOLUMEFMTINTERNAL)RTMemAllocZ(sizeof(RTDVMVOLUMEFMTINTERNAL));

    if (VALID_PTR(pVol))
    {
        pVol->pVolMgr    = pThis;
        pVol->idxEntry   = idx;
        pVol->pGptEntry  = pGptEntry;
        pVol->offStart   = RTDVM_GPT_LBA2BYTE(pGptEntry->u64LbaFirst, pThis->pDisk);
        pVol->cbVolume   = RTDVM_GPT_LBA2BYTE(pGptEntry->u64LbaLast - pGptEntry->u64LbaFirst + 1, pThis->pDisk);

        *phVolFmt = pVol;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

DECLCALLBACK(int) dvmFmtGptQueryFirstVolume(RTDVMFMT hVolMgrFmt, PRTDVMVOLUMEFMT phVolFmt)
{
    int rc = VINF_SUCCESS;
    PRTDVMFMTINTERNAL pThis = hVolMgrFmt;

    if (pThis->cPartitions != 0)
    {
        PGptEntry pGptEntry = &pThis->paGptEntries[0];

        /* Search for the first non empty entry. */
        for (unsigned i = 0; i < pThis->HdrRev1.cPartitionEntries; i++)
        {
            if (!RTUuidIsNull(&pGptEntry->UuidType))
            {
                rc = dvmFmtMbrVolumeCreate(pThis, pGptEntry, i, phVolFmt);
                break;
            }
            pGptEntry++;
        }
    }
    else
        rc = VERR_DVM_MAP_EMPTY;

    return rc;
}

DECLCALLBACK(int) dvmFmtGptQueryNextVolume(RTDVMFMT hVolMgrFmt, RTDVMVOLUMEFMT hVolFmt, PRTDVMVOLUMEFMT phVolFmtNext)
{
    int rc = VERR_DVM_MAP_NO_VOLUME;
    PRTDVMFMTINTERNAL pThis = hVolMgrFmt;
    PRTDVMVOLUMEFMTINTERNAL pVol = hVolFmt;
    PGptEntry pGptEntry = pVol->pGptEntry + 1;

    for (unsigned i = pVol->idxEntry + 1; i < pThis->HdrRev1.cPartitionEntries; i++)
    {
        if (!RTUuidIsNull(&pGptEntry->UuidType))
        {
            rc = dvmFmtMbrVolumeCreate(pThis, pGptEntry, i, phVolFmtNext);
            break;
        }
        pGptEntry++;
    }

    return rc;
}

DECLCALLBACK(void) dvmFmtGptVolumeClose(RTDVMVOLUMEFMT hVolFmt)
{
    PRTDVMVOLUMEFMTINTERNAL pVol = hVolFmt;

    pVol->pVolMgr    = NULL;
    pVol->offStart   = 0;
    pVol->cbVolume   = 0;
    pVol->pGptEntry  = NULL;

    RTMemFree(pVol);
}

DECLCALLBACK(uint64_t) dvmFmtGptVolumeGetSize(RTDVMVOLUMEFMT hVolFmt)
{
    PRTDVMVOLUMEFMTINTERNAL pVol = hVolFmt;

    return pVol->cbVolume;
}

DECLCALLBACK(int) dvmFmtGptVolumeQueryName(RTDVMVOLUMEFMT hVolFmt, char **ppszVolName)
{
    PRTDVMVOLUMEFMTINTERNAL pVol = hVolFmt;
    int rc = VINF_SUCCESS;

    *ppszVolName = NULL;
    rc = RTUtf16ToUtf8Ex(&pVol->pGptEntry->aPartitionName[0], RT_ELEMENTS(pVol->pGptEntry->aPartitionName),
                         ppszVolName, 0, NULL);

    return rc;
}

DECLCALLBACK(RTDVMVOLTYPE) dvmFmtGptVolumeGetType(RTDVMVOLUMEFMT hVolFmt)
{
    RTDVMVOLTYPE enmVolType = RTDVMVOLTYPE_UNKNOWN;
    PRTDVMVOLUMEFMTINTERNAL pVol = hVolFmt;

    for (unsigned i = 0; i < RT_ELEMENTS(g_aPartType2DvmVolTypes); i++)
        if (!RTUuidCompareStr(&pVol->pGptEntry->UuidType, g_aPartType2DvmVolTypes[i].pcszUuid))
        {
            enmVolType = g_aPartType2DvmVolTypes[i].enmVolType;
            break;
        }

    return enmVolType;
}

DECLCALLBACK(uint64_t) dvmFmtGptVolumeGetFlags(RTDVMVOLUMEFMT hVolFmt)
{
    NOREF(hVolFmt); /* No supported flags for now. */
    return 0;
}

DECLCALLBACK(int) dvmFmtGptVolumeRead(RTDVMVOLUMEFMT hVolFmt, uint64_t off, void *pvBuf, size_t cbRead)
{
    PRTDVMVOLUMEFMTINTERNAL pVol = hVolFmt;
    AssertReturn(off + cbRead <= pVol->cbVolume, VERR_INVALID_PARAMETER);

    return dvmDiskRead(pVol->pVolMgr->pDisk, pVol->offStart + off, pvBuf, cbRead);
}

DECLCALLBACK(int) dvmFmtGptVolumeWrite(RTDVMVOLUMEFMT hVolFmt, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    PRTDVMVOLUMEFMTINTERNAL pVol = hVolFmt;
    AssertReturn(off + cbWrite <= pVol->cbVolume, VERR_INVALID_PARAMETER);

    return dvmDiskWrite(pVol->pVolMgr->pDisk, pVol->offStart + off, pvBuf, cbWrite);
}

RTDVMFMTOPS g_DvmFmtGpt = 
{
    /* pcszFmt */
    "GPT",
    /* pfnProbe */
    dvmFmtGptProbe,
    /* pfnOpen */
    dvmFmtGptOpen,
    /* pfnInitialize */
    dvmFmtGptInitialize,
    /* pfnClose */
    dvmFmtGptClose,
    /* pfnGetValidVolumes */
    dvmFmtGptGetValidVolumes,
    /* pfnGetMaxVolumes */
    dvmFmtGptGetMaxVolumes,
    /* pfnQueryFirstVolume */
    dvmFmtGptQueryFirstVolume,
    /* pfnQueryNextVolume */
    dvmFmtGptQueryNextVolume,
    /* pfnVolumeClose */
    dvmFmtGptVolumeClose,
    /* pfnVolumeGetSize */
    dvmFmtGptVolumeGetSize,
    /* pfnVolumeQueryName */
    dvmFmtGptVolumeQueryName,
    /* pfnVolumeGetType */
    dvmFmtGptVolumeGetType,
    /* pfnVolumeGetFlags */
    dvmFmtGptVolumeGetFlags,
    /* pfnVolumeRead */
    dvmFmtGptVolumeRead,
    /* pfnVolumeWrite */
    dvmFmtGptVolumeWrite
};

