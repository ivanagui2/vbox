/* $Id$ */
/** @file
 * IPRT - Path manipulation.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_PATH
#include <Windows.h>

#include <iprt/path.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/log.h>
#include <iprt/err.h>
#include "internal/path.h"
#include "internal/fs.h"


/**
 * Get the real (no symlinks, no . or .. components) path, must exist.
 *
 * @returns iprt status code.
 * @param   pszPath         The path to resolve.
 * @param   pszRealPath     Where to store the real path.
 * @param   cchRealPath     Size of the buffer.
 */
RTDECL(int) RTPathReal(const char *pszPath, char *pszRealPath, size_t cchRealPath)
{
    /*
     * Convert to UTF-16, call Win32 APIs, convert back.
     */
    PRTUTF16 pwszPath;
    int rc = RTStrToUtf16(pszPath, &pwszPath);
    if (!RT_SUCCESS(rc))
        return (rc);

    LPWSTR lpFile;
    WCHAR  wsz[RTPATH_MAX];
    rc = GetFullPathNameW((LPCWSTR)pwszPath, RT_ELEMENTS(wsz), &wsz[0], &lpFile);
    if (rc > 0 && rc < RT_ELEMENTS(wsz))
    {
        /* Check that it exists. (Use RTPathAbs() to just resolve the name.) */
        DWORD dwAttr = GetFileAttributesW(wsz);
        if (dwAttr != INVALID_FILE_ATTRIBUTES)
            rc = RTUtf16ToUtf8Ex((PRTUTF16)&wsz[0], RTSTR_MAX, &pszRealPath, cchRealPath, NULL);
        else
            rc = RTErrConvertFromWin32(GetLastError());
    }
    else if (rc <= 0)
        rc = RTErrConvertFromWin32(GetLastError());
    else
        rc = VERR_FILENAME_TOO_LONG;

    RTUtf16Free(pwszPath);

    return rc;
}


/**
 * Get the absolute path (no symlinks, no . or .. components), doesn't have to exit.
 *
 * @returns iprt status code.
 * @param   pszPath         The path to resolve.
 * @param   pszAbsPath      Where to store the absolute path.
 * @param   cchAbsPath      Size of the buffer.
 */
RTDECL(int) RTPathAbs(const char *pszPath, char *pszAbsPath, size_t cchAbsPath)
{
    /*
     * Validation.
     */
    AssertPtr(pszAbsPath);
    AssertPtr(pszPath);
    if (RT_UNLIKELY(!*pszPath))
        return VERR_INVALID_PARAMETER;

    /*
     * Convert to UTF-16, call Win32 API, convert back.
     */
    LPWSTR pwszPath;
    int rc = RTStrToUtf16(pszPath, &pwszPath);
    if (!RT_SUCCESS(rc))
        return (rc);

    LPWSTR pwszFile; /* Ignored */
    RTUTF16 wsz[RTPATH_MAX];
    rc = GetFullPathNameW(pwszPath, RT_ELEMENTS(wsz), &wsz[0], &pwszFile);
    if (rc > 0 && rc < RT_ELEMENTS(wsz))
    {
        size_t cch;
        rc = RTUtf16ToUtf8Ex(&wsz[0], RTSTR_MAX, &pszAbsPath, cchAbsPath, &cch);
        if (RT_SUCCESS(rc))
        {
            /*
             * Remove trailing slash if the path may be pointing to a directory.
             * (See posix variant.)
             */
            if (    cch > 1
                &&  RTPATH_IS_SLASH(pszAbsPath[cch - 1])
                &&  !RTPATH_IS_VOLSEP(pszAbsPath[cch - 2])
                &&  !RTPATH_IS_SLASH(pszAbsPath[cch - 2]))
                pszAbsPath[cch - 1] = '\0';
        }
    }
    else if (rc <= 0)
        rc = RTErrConvertFromWin32(GetLastError());
    else
        rc = VERR_FILENAME_TOO_LONG;

    RTUtf16Free(pwszPath);
    return rc;
}


/**
 * Gets the user home directory.
 *
 * @returns iprt status code.
 * @param   pszPath     Buffer where to store the path.
 * @param   cchPath     Buffer size in bytes.
 */
RTDECL(int) RTPathUserHome(char *pszPath, size_t cchPath)
{
    RTUTF16 wszPath[RTPATH_MAX];
    DWORD   dwAttr;

    /*
     * There are multiple definitions for what WE think of as user home...
     */
    if (    !GetEnvironmentVariableW(L"HOME", &wszPath[0], RTPATH_MAX)
        ||  (dwAttr = GetFileAttributesW(&wszPath[0])) == INVALID_FILE_ATTRIBUTES
        ||  !(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        if (    !GetEnvironmentVariableW(L"USERPROFILE", &wszPath[0], RTPATH_MAX)
            ||  (dwAttr = GetFileAttributesW(&wszPath[0])) == INVALID_FILE_ATTRIBUTES
            ||  !(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
        {
            /* %HOMEDRIVE%%HOMEPATH% */
            if (!GetEnvironmentVariableW(L"HOMEDRIVE", &wszPath[0], RTPATH_MAX))
                return VERR_PATH_NOT_FOUND;
            size_t const cwc = RTUtf16Len(&wszPath[0]);
            if (    !GetEnvironmentVariableW(L"HOMEPATH", &wszPath[cwc], RTPATH_MAX - (DWORD)cwc)
                ||  (dwAttr = GetFileAttributesW(&wszPath[0])) == INVALID_FILE_ATTRIBUTES
                ||  !(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
                return VERR_PATH_NOT_FOUND;
        }
    }

    /*
     * Convert and return.
     */
    return RTUtf16ToUtf8Ex(&wszPath[0], RTSTR_MAX, &pszPath, cchPath, NULL);
}


RTR3DECL(int) RTPathQueryInfo(const char *pszPath, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAdditionalAttribs)
{
    return RTPathQueryInfoEx(pszPath, pObjInfo, enmAdditionalAttribs, RTPATH_F_ON_LINK);
}


RTR3DECL(int) RTPathQueryInfoEx(const char *pszPath, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAdditionalAttribs, uint32_t fFlags)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(*pszPath, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pObjInfo, VERR_INVALID_POINTER);
    AssertMsgReturn(    enmAdditionalAttribs >= RTFSOBJATTRADD_NOTHING
                    &&  enmAdditionalAttribs <= RTFSOBJATTRADD_LAST,
                    ("Invalid enmAdditionalAttribs=%p\n", enmAdditionalAttribs),
                    VERR_INVALID_PARAMETER);
    AssertMsgReturn(RTPATH_F_IS_VALID(fFlags, 0), ("%#x\n", fFlags), VERR_INVALID_PARAMETER);

    /*
     * Query file info.
     */
    WIN32_FILE_ATTRIBUTE_DATA Data;
#ifndef RT_DONT_CONVERT_FILENAMES
    PRTUTF16 pwszPath;
    int rc = RTStrToUtf16(pszPath, &pwszPath);
    if (RT_FAILURE(rc))
        return rc;
    if (!GetFileAttributesExW(pwszPath, GetFileExInfoStandard, &Data))
    {
        /* Fallback to FindFileFirst in case of sharing violation. */
        if (GetLastError() == ERROR_SHARING_VIOLATION)
        {
            WIN32_FIND_DATAW FindData;
            HANDLE hDir = FindFirstFileW(pwszPath, &FindData);
            if (hDir == INVALID_HANDLE_VALUE)
            {
                rc = RTErrConvertFromWin32(GetLastError());
                RTUtf16Free(pwszPath);
                return rc;
            }
            FindClose(hDir);
            Data.dwFileAttributes = FindData.dwFileAttributes;
            Data.ftCreationTime = FindData.ftCreationTime;
            Data.ftLastAccessTime = FindData.ftLastAccessTime;
            Data.ftLastWriteTime = FindData.ftLastWriteTime;
            Data.nFileSizeHigh = FindData.nFileSizeHigh;
            Data.nFileSizeLow = FindData.nFileSizeLow;
        }
        else
        {
            rc = RTErrConvertFromWin32(GetLastError());
            RTUtf16Free(pwszPath);
            return rc;
        }
    }
    RTUtf16Free(pwszPath);
#else
    if (!GetFileAttributesExA(pszPath, GetFileExInfoStandard, &Data))
    {
        /* Fallback to FindFileFirst in case of sharing violation. */
        if (GetLastError() != ERROR_SHARING_VIOLATION)
            return RTErrConvertFromWin32(GetLastError());
        WIN32_FIND_DATAA FindData;
        HANDLE hDir = FindFirstFileA(pszPath, &FindData);
        if (hDir == INVALID_HANDLE_VALUE)
            return RTErrConvertFromWin32(GetLastError());
        FindClose(hDir);
        Data.dwFileAttributes = FindData.dwFileAttributes;
        Data.ftCreationTime = FindData.ftCreationTime;
        Data.ftLastAccessTime = FindData.ftLastAccessTime;
        Data.ftLastWriteTime = FindData.ftLastWriteTime;
        Data.nFileSizeHigh = FindData.nFileSizeHigh;
        Data.nFileSizeLow = FindData.nFileSizeLow;
    }
#endif
    if (   (fFlags & RTPATH_F_FOLLOW_LINK)
        && (Data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
    {
#ifndef DEBUG_sandervl
        AssertFailed();
#endif
        /** @todo Symlinks: RTPathQueryInfoEx is not handling symbolic links
         *        correctly on Windows.  (Both GetFileAttributesEx and FileFindFirst
         *        will return info about the symlink.) */
    }

    /*
     * Setup the returned data.
     */
    pObjInfo->cbObject    = ((uint64_t)Data.nFileSizeHigh << 32)
                          |  (uint64_t)Data.nFileSizeLow;
    pObjInfo->cbAllocated = pObjInfo->cbObject;

    Assert(sizeof(uint64_t) == sizeof(Data.ftCreationTime));
    RTTimeSpecSetNtTime(&pObjInfo->BirthTime,         *(uint64_t *)&Data.ftCreationTime);
    RTTimeSpecSetNtTime(&pObjInfo->AccessTime,        *(uint64_t *)&Data.ftLastAccessTime);
    RTTimeSpecSetNtTime(&pObjInfo->ModificationTime,  *(uint64_t *)&Data.ftLastWriteTime);
    pObjInfo->ChangeTime  = pObjInfo->ModificationTime;

    pObjInfo->Attr.fMode  = rtFsModeFromDos((Data.dwFileAttributes << RTFS_DOS_SHIFT) & RTFS_DOS_MASK_NT,
                                            pszPath, strlen(pszPath));

    /*
     * Requested attributes (we cannot provide anything actually).
     */
    switch (enmAdditionalAttribs)
    {
        case RTFSOBJATTRADD_EASIZE:
            pObjInfo->Attr.enmAdditional          = RTFSOBJATTRADD_EASIZE;
            pObjInfo->Attr.u.EASize.cb            = 0;
            break;

        case RTFSOBJATTRADD_UNIX:
            pObjInfo->Attr.enmAdditional          = RTFSOBJATTRADD_UNIX;
            pObjInfo->Attr.u.Unix.uid             = ~0U;
            pObjInfo->Attr.u.Unix.gid             = ~0U;
            pObjInfo->Attr.u.Unix.cHardlinks      = 1;
            pObjInfo->Attr.u.Unix.INodeIdDevice   = 0; /** @todo use volume serial number */
            pObjInfo->Attr.u.Unix.INodeId         = 0; /** @todo use fileid (see GetFileInformationByHandle). */
            pObjInfo->Attr.u.Unix.fFlags          = 0;
            pObjInfo->Attr.u.Unix.GenerationId    = 0;
            pObjInfo->Attr.u.Unix.Device          = 0;
            break;

        case RTFSOBJATTRADD_NOTHING:
            pObjInfo->Attr.enmAdditional          = RTFSOBJATTRADD_NOTHING;
            break;

        default:
            AssertMsgFailed(("Impossible!\n"));
            return VERR_INTERNAL_ERROR;
    }

    return VINF_SUCCESS;
}


RTR3DECL(int) RTPathSetTimes(const char *pszPath, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                             PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    return RTPathSetTimesEx(pszPath, pAccessTime, pModificationTime, pChangeTime, pBirthTime, RTPATH_F_ON_LINK);
}


RTR3DECL(int) RTPathSetTimesEx(const char *pszPath, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                               PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime, uint32_t fFlags)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(*pszPath, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pAccessTime, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pModificationTime, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pChangeTime, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pBirthTime, VERR_INVALID_POINTER);
    AssertMsgReturn(RTPATH_F_IS_VALID(fFlags, 0), ("%#x\n", fFlags), VERR_INVALID_PARAMETER);

    /*
     * Convert the path.
     */
    PRTUTF16 pwszPath;
    int rc = RTStrToUtf16(pszPath, &pwszPath);
    if (RT_SUCCESS(rc))
    {
        HANDLE hFile;
        if (fFlags & RTPATH_F_FOLLOW_LINK)
            hFile = CreateFileW(pwszPath,
                                FILE_WRITE_ATTRIBUTES,   /* dwDesiredAccess */
                                FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, /* dwShareMode */
                                NULL,                    /* security attribs */
                                OPEN_EXISTING,           /* dwCreationDisposition */
                                FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_NORMAL,
                                NULL);
        else
        {
/** @todo Symlink: Test RTPathSetTimesEx on Windows. (The code is disabled
 *        because it's not tested yet.) */
#if 0 //def FILE_FLAG_OPEN_REPARSE_POINT
            hFile = CreateFileW(pwszPath,
                                FILE_WRITE_ATTRIBUTES,   /* dwDesiredAccess */
                                FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, /* dwShareMode */
                                NULL,                    /* security attribs */
                                OPEN_EXISTING,           /* dwCreationDisposition */
                                FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
                                NULL);

            if (hFile == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER)
#endif
                hFile = CreateFileW(pwszPath,
                                    FILE_WRITE_ATTRIBUTES,   /* dwDesiredAccess */
                                    FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, /* dwShareMode */
                                    NULL,                    /* security attribs */
                                    OPEN_EXISTING,           /* dwCreationDisposition */
                                    FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_NORMAL,
                                    NULL);
        }
        if (hFile != INVALID_HANDLE_VALUE)
        {
            /*
             * Check if it's a no-op.
             */
            if (!pAccessTime && !pModificationTime && !pBirthTime)
                rc = VINF_SUCCESS;    /* NOP */
            else
            {
                /*
                 * Convert the input and call the API.
                 */
                FILETIME    CreationTimeFT;
                PFILETIME   pCreationTimeFT = NULL;
                if (pBirthTime)
                    pCreationTimeFT = RTTimeSpecGetNtFileTime(pBirthTime, &CreationTimeFT);

                FILETIME    LastAccessTimeFT;
                PFILETIME   pLastAccessTimeFT = NULL;
                if (pAccessTime)
                    pLastAccessTimeFT = RTTimeSpecGetNtFileTime(pAccessTime, &LastAccessTimeFT);

                FILETIME    LastWriteTimeFT;
                PFILETIME   pLastWriteTimeFT = NULL;
                if (pModificationTime)
                    pLastWriteTimeFT = RTTimeSpecGetNtFileTime(pModificationTime, &LastWriteTimeFT);

                if (SetFileTime(hFile, pCreationTimeFT, pLastAccessTimeFT, pLastWriteTimeFT))
                    rc = VINF_SUCCESS;
                else
                {
                    DWORD Err = GetLastError();
                    rc = RTErrConvertFromWin32(Err);
                    Log(("RTPathSetTimes('%s', %p, %p, %p, %p): SetFileTime failed with lasterr %d (%Rrc)\n",
                         pszPath, pAccessTime, pModificationTime, pChangeTime, pBirthTime, Err, rc));
                }
            }
            BOOL fRc = CloseHandle(hFile); Assert(fRc); NOREF(fRc);
        }
        else
        {
            DWORD Err = GetLastError();
            rc = RTErrConvertFromWin32(Err);
            Log(("RTPathSetTimes('%s',,,,): failed with %Rrc and lasterr=%u\n", pszPath, rc, Err));
        }

        RTUtf16Free(pwszPath);
    }

    LogFlow(("RTPathSetTimes(%p:{%s}, %p:{%RDtimespec}, %p:{%RDtimespec}, %p:{%RDtimespec}, %p:{%RDtimespec}): return %Rrc\n",
             pszPath, pszPath, pAccessTime, pAccessTime, pModificationTime, pModificationTime,
             pChangeTime, pChangeTime, pBirthTime, pBirthTime));
    return rc;
}




/**
 * Internal worker for RTFileRename and RTFileMove.
 *
 * @returns iprt status code.
 * @param   pszSrc      The source filename.
 * @param   pszDst      The destination filename.
 * @param   fFlags      The windows MoveFileEx flags.
 * @param   fFileType   The filetype. We use the RTFMODE filetypes here. If it's 0,
 *                      anything goes. If it's RTFS_TYPE_DIRECTORY we'll check that the
 *                      source is a directory. If Its RTFS_TYPE_FILE we'll check that it's
 *                      not a directory (we are NOT checking whether it's a file).
 */
int rtPathWin32MoveRename(const char *pszSrc, const char *pszDst, uint32_t fFlags, RTFMODE fFileType)
{
    /*
     * Convert the strings.
     */
    PRTUTF16 pwszSrc;
    int rc = RTStrToUtf16(pszSrc, &pwszSrc);
    if (RT_SUCCESS(rc))
    {
        PRTUTF16 pwszDst;
        rc = RTStrToUtf16(pszDst, &pwszDst);
        if (RT_SUCCESS(rc))
        {
            /*
             * Check object type if requested.
             * This is open to race conditions.
             */
            if (fFileType)
            {
                DWORD dwAttr = GetFileAttributesW(pwszSrc);
                if (dwAttr == INVALID_FILE_ATTRIBUTES)
                    rc = RTErrConvertFromWin32(GetLastError());
                else if (RTFS_IS_DIRECTORY(fFileType))
                    rc = dwAttr & FILE_ATTRIBUTE_DIRECTORY ? VINF_SUCCESS : VERR_NOT_A_DIRECTORY;
                else
                    rc = dwAttr & FILE_ATTRIBUTE_DIRECTORY ? VERR_IS_A_DIRECTORY : VINF_SUCCESS;
            }
            if (RT_SUCCESS(rc))
            {
                if (MoveFileExW(pwszSrc, pwszDst, fFlags))
                    rc = VINF_SUCCESS;
                else
                {
                    DWORD Err = GetLastError();
                    rc = RTErrConvertFromWin32(Err);
                    Log(("MoveFileExW('%s', '%s', %#x, %RTfmode): fails with rc=%Rrc & lasterr=%d\n",
                         pszSrc, pszDst, fFlags, fFileType, rc, Err));
                }
            }
            RTUtf16Free(pwszDst);
        }
        RTUtf16Free(pwszSrc);
    }
    return rc;
}


RTR3DECL(int) RTPathRename(const char *pszSrc, const char *pszDst, unsigned fRename)
{
    /*
     * Validate input.
     */
    AssertMsgReturn(VALID_PTR(pszSrc), ("%p\n", pszSrc), VERR_INVALID_POINTER);
    AssertMsgReturn(VALID_PTR(pszDst), ("%p\n", pszDst), VERR_INVALID_POINTER);
    AssertMsgReturn(*pszSrc, ("%p\n", pszSrc), VERR_INVALID_PARAMETER);
    AssertMsgReturn(*pszDst, ("%p\n", pszDst), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!(fRename & ~RTPATHRENAME_FLAGS_REPLACE), ("%#x\n", fRename), VERR_INVALID_PARAMETER);

    /*
     * Call the worker.
     */
    int rc = rtPathWin32MoveRename(pszSrc, pszDst, fRename & RTPATHRENAME_FLAGS_REPLACE ? MOVEFILE_REPLACE_EXISTING : 0, 0);

    LogFlow(("RTPathRename(%p:{%s}, %p:{%s}, %#x): returns %Rrc\n", pszSrc, pszSrc, pszDst, pszDst, fRename, rc));
    return rc;
}


RTDECL(bool) RTPathExists(const char *pszPath)
{
    return RTPathExistsEx(pszPath, RTPATH_F_FOLLOW_LINK);
}


RTDECL(bool) RTPathExistsEx(const char *pszPath, uint32_t fFlags)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pszPath, false);
    AssertReturn(*pszPath, false);
    Assert(RTPATH_F_IS_VALID(fFlags, 0));

    /*
     * Try query file info.
     */
    DWORD dwAttr;
#ifndef RT_DONT_CONVERT_FILENAMES
    PRTUTF16 pwszPath;
    int rc = RTStrToUtf16(pszPath, &pwszPath);
    if (RT_SUCCESS(rc))
    {
        dwAttr = GetFileAttributesW(pwszPath);
        RTUtf16Free(pwszPath);
    }
    else
        dwAttr = INVALID_FILE_ATTRIBUTES;
#else
    dwAttr = GetFileAttributesA(pszPath);
#endif

    if (dwAttr == INVALID_FILE_ATTRIBUTES)
        return false;

#ifdef FILE_ATTRIBUTE_REPARSE_POINT
    if (   (fFlags & RTPATH_F_FOLLOW_LINK)
        && (dwAttr & FILE_ATTRIBUTE_REPARSE_POINT))
    {
        AssertFailed();
        /** @todo Symlinks: RTPathExists+RTPathExistsEx is misbehaving on symbolic
         *        links on Windows. */
    }
#endif

    return true;
}


RTDECL(int) RTPathGetCurrent(char *pszPath, size_t cchPath)
{
    int rc;

    /*
     * GetCurrentDirectory may in some cases omit the drive letter, according
     * to MSDN, thus the GetFullPathName call.
     */
#ifndef RT_DONT_CONVERT_FILENAMES
    RTUTF16 wszCurPath[RTPATH_MAX];
    if (GetCurrentDirectoryW(RTPATH_MAX, wszCurPath))
    {
        RTUTF16 wszFullPath[RTPATH_MAX];
        if (GetFullPathNameW(wszCurPath, RTPATH_MAX, wszFullPath, NULL))
            rc = RTUtf16ToUtf8Ex(&wszFullPath[0], RTSTR_MAX, &pszPath, cchPath, NULL);
        else
            rc = RTErrConvertFromWin32(GetLastError());
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());
#else
    char szCurPath[RTPATH_MAX];
    if (GetCurrentDirectory(RTPATH_MAX, szCurPath))
    {
        if (GetFullPathName(szCurPath, cchPath, pszPath, NULL))
            rc = VINF_SUCCESS;
        else
            rc = RTErrConvertFromWin32(GetLastError());
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());
#endif
    return rc;
}


RTDECL(int) RTPathSetCurrent(const char *pszPath)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(*pszPath, VERR_INVALID_PARAMETER);

    /*
     * This interface is almost identical to the Windows API.
     */
#ifndef RT_DONT_CONVERT_FILENAMES
    PRTUTF16 pwszPath;
    int rc = RTStrToUtf16(pszPath, &pwszPath);
    if (RT_SUCCESS(rc))
    {
        /** @todo improove the slash stripping a bit? */
        size_t cwc = RTUtf16Len(pwszPath);
        if (    cwc >= 2
            &&  (   pwszPath[cwc - 1] == L'/'
                 || pwszPath[cwc - 1] == L'\\')
            &&  pwszPath[cwc - 2] != ':')
            pwszPath[cwc - 1] = L'\0';

        if (!SetCurrentDirectoryW(pwszPath))
            rc = RTErrConvertFromWin32(GetLastError());

        RTUtf16Free(pwszPath);
    }
#else
    int rc = VINF_SUCCESS;
    /** @todo improove the slash stripping a bit? */
    char const *pszEnd = strchr(pszPath, '\0');
    size_t const cchPath = pszPath - pszEnd;
    if (    cchPath >= 2
        &&  (   pszEnd[-1] == '/'
             || pszEnd[-1] == '\\')
        &&  pszEnd[-2] == ':')
    {
        char *pszCopy = (char *)RTMemTmpAlloc(cchPath);
        if (pszCopy)
        {
            memcpy(pszCopy, pszPath, cchPath - 1);
            pszCopy[cchPath - 1] = '\0';
            if (!SetCurrentDirectory(pszCopy))
                rc = RTErrConvertFromWin32(GetLastError());
            RTMemTmpFree(pszCopy);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
    {
        if (!SetCurrentDirectory(pszPath))
            rc = RTErrConvertFromWin32(GetLastError());
    }
#endif
    return rc;
}

