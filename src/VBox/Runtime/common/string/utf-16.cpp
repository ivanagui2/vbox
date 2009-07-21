/* $Id$ */
/** @file
 * IPRT - UTF-16.
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
#include <iprt/string.h>
#include "internal/iprt.h"

#include <iprt/uni.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include "internal/string.h"



RTDECL(void)  RTUtf16Free(PRTUTF16 pwszString)
{
    if (pwszString)
        RTMemTmpFree(pwszString);
}
RT_EXPORT_SYMBOL(RTUtf16Free);


RTDECL(PRTUTF16) RTUtf16Dup(PCRTUTF16 pwszString)
{
    Assert(pwszString);
    size_t cb = (RTUtf16Len(pwszString) + 1) * sizeof(RTUTF16);
    PRTUTF16 pwsz = (PRTUTF16)RTMemAlloc(cb);
    if (pwsz)
        memcpy(pwsz, pwszString, cb);
    return pwsz;
}
RT_EXPORT_SYMBOL(RTUtf16Dup);


RTDECL(int) RTUtf16DupEx(PRTUTF16 *ppwszString, PCRTUTF16 pwszString, size_t cwcExtra)
{
    Assert(pwszString);
    size_t cb = (RTUtf16Len(pwszString) + 1) * sizeof(RTUTF16);
    PRTUTF16 pwsz = (PRTUTF16)RTMemAlloc(cb + cwcExtra * sizeof(RTUTF16));
    if (pwsz)
    {
        memcpy(pwsz, pwszString, cb);
        *ppwszString = pwsz;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}
RT_EXPORT_SYMBOL(RTUtf16DupEx);


RTDECL(size_t) RTUtf16Len(PCRTUTF16 pwszString)
{
    if (!pwszString)
        return 0;

    PCRTUTF16 pwsz = pwszString;
    while (*pwsz)
        pwsz++;
    return pwsz - pwszString;
}
RT_EXPORT_SYMBOL(RTUtf16Len);


RTDECL(int) RTUtf16Cmp(register PCRTUTF16 pwsz1, register PCRTUTF16 pwsz2)
{
    if (pwsz1 == pwsz2)
        return 0;
    if (!pwsz1)
        return -1;
    if (!pwsz2)
        return 1;

    for (;;)
    {
        register RTUTF16  wcs = *pwsz1;
        register int     iDiff = wcs - *pwsz2;
        if (iDiff || !wcs)
            return iDiff;
        pwsz1++;
        pwsz2++;
    }
}
RT_EXPORT_SYMBOL(RTUtf16Cmp);


RTDECL(int) RTUtf16ICmp(register PCRTUTF16 pwsz1, register PCRTUTF16 pwsz2)
{
    if (pwsz1 == pwsz2)
        return 0;
    if (!pwsz1)
        return -1;
    if (!pwsz2)
        return 1;

    PCRTUTF16 pwsz1Start = pwsz1; /* keep it around in case we have to backtrack on a surrogate pair */
    for (;;)
    {
        register RTUTF16  wc1 = *pwsz1;
        register RTUTF16  wc2 = *pwsz2;
        register int     iDiff = wc1 - wc2;
        if (iDiff)
        {
            /* unless they are *both* surrogate pairs, there is no chance they'll be identical. */
            if (    wc1 < 0xd800
                ||  wc2 < 0xd800
                ||  wc1 > 0xdfff
                ||  wc2 > 0xdfff)
            {
                /* simple UCS-2 char */
                iDiff = RTUniCpToUpper(wc1) - RTUniCpToUpper(wc2);
                if (iDiff)
                    iDiff = RTUniCpToLower(wc1) - RTUniCpToLower(wc2);
            }
            else
            {
                /* a damned pair */
                RTUNICP uc1;
                RTUNICP uc2;
                if (wc1 >= 0xdc00)
                {
                    if (pwsz1Start == pwsz1)
                        return iDiff;
                    uc1 = pwsz1[-1];
                    if (uc1 < 0xd800 || uc1 >= 0xdc00)
                        return iDiff;
                    uc1 = 0x10000 + (((uc1       & 0x3ff) << 10) | (wc1 & 0x3ff));
                    uc2 = 0x10000 + (((pwsz2[-1] & 0x3ff) << 10) | (wc2 & 0x3ff));
                }
                else
                {
                    uc1 = *++pwsz1;
                    if (uc1 < 0xdc00 || uc1 >= 0xe000)
                        return iDiff;
                    uc1 = 0x10000 + (((wc1 & 0x3ff) << 10) | (uc1      & 0x3ff));
                    uc2 = 0x10000 + (((wc2 & 0x3ff) << 10) | (*++pwsz2 & 0x3ff));
                }
                iDiff = RTUniCpToUpper(uc1) - RTUniCpToUpper(uc2);
                if (iDiff)
                    iDiff = RTUniCpToLower(uc1) - RTUniCpToLower(uc2); /* serious paranoia! */
            }
            if (iDiff)
                return iDiff;
        }
        if (!wc1)
            return 0;
        pwsz1++;
        pwsz2++;
    }
}
RT_EXPORT_SYMBOL(RTUtf16ICmp);


RTDECL(PRTUTF16) RTUtf16ToLower(PRTUTF16 pwsz)
{
    PRTUTF16 pwc = pwsz;
    for (;;)
    {
        RTUTF16 wc = *pwc;
        if (!wc)
            break;
        if (wc < 0xd800 || wc >= 0xdc00)
        {
            RTUNICP ucFolded = RTUniCpToLower(wc);
            if (ucFolded < 0x10000)
                *pwc++ = RTUniCpToLower(wc);
        }
        else
        {
            /* surrogate */
            RTUTF16 wc2 = pwc[1];
            if (wc2 >= 0xdc00 && wc2 <= 0xdfff)
            {
                RTUNICP uc = 0x10000 + (((wc & 0x3ff) << 10) | (wc2 & 0x3ff));
                RTUNICP ucFolded = RTUniCpToLower(uc);
                if (uc != ucFolded && ucFolded >= 0x10000) /* we don't support shrinking the string */
                {
                    uc -= 0x10000;
                    *pwc++ = 0xd800 | (uc >> 10);
                    *pwc++ = 0xdc00 | (uc & 0x3ff);
                }
            }
            else /* invalid encoding. */
                pwc++;
        }
    }
    return pwsz;
}
RT_EXPORT_SYMBOL(RTUtf16ToLower);


RTDECL(PRTUTF16) RTUtf16ToUpper(PRTUTF16 pwsz)
{
    PRTUTF16 pwc = pwsz;
    for (;;)
    {
        RTUTF16 wc = *pwc;
        if (!wc)
            break;
        if (wc < 0xd800 || wc >= 0xdc00)
            *pwc++ = RTUniCpToUpper(wc);
        else
        {
            /* surrogate */
            RTUTF16 wc2 = pwc[1];
            if (wc2 >= 0xdc00 && wc2 <= 0xdfff)
            {
                RTUNICP uc = 0x10000 + (((wc & 0x3ff) << 10) | (wc2 & 0x3ff));
                RTUNICP ucFolded = RTUniCpToUpper(uc);
                if (uc != ucFolded && ucFolded >= 0x10000) /* we don't support shrinking the string */
                {
                    uc -= 0x10000;
                    *pwc++ = 0xd800 | (uc >> 10);
                    *pwc++ = 0xdc00 | (uc & 0x3ff);
                }
            }
            else /* invalid encoding. */
                pwc++;
        }
    }
    return pwsz;
}
RT_EXPORT_SYMBOL(RTUtf16ToUpper);


/**
 * Validate the UTF-16 encoding and calculates the length of an UTF-8 encoding.
 *
 * @returns iprt status code.
 * @param   pwsz        The UTF-16 string.
 * @param   cwc         The max length of the UTF-16 string to consider.
 * @param   pcch        Where to store the length (excluding '\\0') of the UTF-8 string. (cch == cb, btw)
 */
static int rtUtf16CalcUtf8Length(PCRTUTF16 pwsz, size_t cwc, size_t *pcch)
{
    int     rc = VINF_SUCCESS;
    size_t  cch = 0;
    while (cwc > 0)
    {
        RTUTF16 wc = *pwsz++; cwc--;
        if (!wc)
            break;
        else if (wc < 0xd800 || wc > 0xdfff)
        {
            if (wc < 0x80)
                cch++;
            else if (wc < 0x800)
                cch += 2;
            else if (wc < 0xfffe)
                cch += 3;
            else
            {
                RTStrAssertMsgFailed(("endian indicator! wc=%#x\n", wc));
                rc = VERR_CODE_POINT_ENDIAN_INDICATOR;
                break;
            }
        }
        else
        {
            if (wc >= 0xdc00)
            {
                RTStrAssertMsgFailed(("Wrong 1st char in surrogate! wc=%#x\n", wc));
                rc = VERR_INVALID_UTF16_ENCODING;
                break;
            }
            if (cwc <= 0)
            {
                RTStrAssertMsgFailed(("Invalid length! wc=%#x\n", wc));
                rc = VERR_INVALID_UTF16_ENCODING;
                break;
            }
            wc = *pwsz++; cwc--;
            if (wc < 0xdc00 || wc > 0xdfff)
            {
                RTStrAssertMsgFailed(("Wrong 2nd char in surrogate! wc=%#x\n", wc));
                rc = VERR_INVALID_UTF16_ENCODING;
                break;
            }
            cch += 4;
        }
    }


    /* done */
    *pcch = cch;
    return rc;
}


/**
 * Recodes an valid UTF-16 string as UTF-8.
 *
 * @returns iprt status code.
 * @param   pwsz        The UTF-16 string.
 * @param   cwc         The number of RTUTF16 characters to process from pwsz. The recoding
 *                      will stop when cwc or '\\0' is reached.
 * @param   psz         Where to store the UTF-8 string.
 * @param   cch         The size of the UTF-8 buffer, excluding the terminator.
 * @param   pcch        Where to store the number of octets actually encoded.
 */
static int rtUtf16RecodeAsUtf8(PCRTUTF16 pwsz, size_t cwc, char *psz, size_t cch, size_t *pcch)
{
    unsigned char  *pwch = (unsigned char *)psz;
    int             rc = VINF_SUCCESS;
    while (cwc > 0)
    {
        RTUTF16 wc = *pwsz++; cwc--;
        if (!wc)
            break;
        else if (wc < 0xd800 || wc > 0xdfff)
        {
            if (wc < 0x80)
            {
                if (RT_UNLIKELY(cch < 1))
                {
                    RTStrAssertMsgFailed(("Buffer overflow! 1\n"));
                    rc = VERR_BUFFER_OVERFLOW;
                    break;
                }
                cch--;
                *pwch++ = (unsigned char)wc;
            }
            else if (wc < 0x800)
            {
                if (RT_UNLIKELY(cch < 2))
                {
                    RTStrAssertMsgFailed(("Buffer overflow! 2\n"));
                    rc = VERR_BUFFER_OVERFLOW;
                    break;
                }
                cch -= 2;
                *pwch++ = 0xc0 | (wc >> 6);
                *pwch++ = 0x80 | (wc & 0x3f);
            }
            else if (wc < 0xfffe)
            {
                if (RT_UNLIKELY(cch < 3))
                {
                    RTStrAssertMsgFailed(("Buffer overflow! 3\n"));
                    rc = VERR_BUFFER_OVERFLOW;
                    break;
                }
                cch -= 3;
                *pwch++ = 0xe0 | (wc >> 12);
                *pwch++ = 0x80 | ((wc >> 6) & 0x3f);
                *pwch++ = 0x80 | (wc & 0x3f);
            }
            else
            {
                RTStrAssertMsgFailed(("endian indicator! wc=%#x\n", wc));
                rc = VERR_CODE_POINT_ENDIAN_INDICATOR;
                break;
            }
        }
        else
        {
            if (wc >= 0xdc00)
            {
                RTStrAssertMsgFailed(("Wrong 1st char in surrogate! wc=%#x\n", wc));
                rc = VERR_INVALID_UTF16_ENCODING;
                break;
            }
            if (cwc <= 0)
            {
                RTStrAssertMsgFailed(("Invalid length! wc=%#x\n", wc));
                rc = VERR_INVALID_UTF16_ENCODING;
                break;
            }
            RTUTF16 wc2 = *pwsz++; cwc--;
            if (wc2 < 0xdc00 || wc2 > 0xdfff)
            {
                RTStrAssertMsgFailed(("Wrong 2nd char in surrogate! wc=%#x\n", wc));
                rc = VERR_INVALID_UTF16_ENCODING;
                break;
            }
            uint32_t CodePoint = 0x10000
                               + (  ((wc & 0x3ff) << 10)
                                  | (wc2 & 0x3ff));
            if (RT_UNLIKELY(cch < 4))
            {
                RTStrAssertMsgFailed(("Buffer overflow! 4\n"));
                rc = VERR_BUFFER_OVERFLOW;
                break;
            }
            cch -= 4;
            *pwch++ = 0xf0 | (CodePoint >> 18);
            *pwch++ = 0x80 | ((CodePoint >> 12) & 0x3f);
            *pwch++ = 0x80 | ((CodePoint >>  6) & 0x3f);
            *pwch++ = 0x80 | (CodePoint & 0x3f);
        }
    }

    /* done */
    *pwch = '\0';
    *pcch = (char *)pwch - psz;
    return rc;
}



RTDECL(int)  RTUtf16ToUtf8(PCRTUTF16 pwszString, char **ppszString)
{
    /*
     * Validate input.
     */
    Assert(VALID_PTR(ppszString));
    Assert(VALID_PTR(pwszString));
    *ppszString = NULL;

    /*
     * Validate the UTF-16 string and calculate the length of the UTF-8 encoding of it.
     */
    size_t cch;
    int rc = rtUtf16CalcUtf8Length(pwszString, RTSTR_MAX, &cch);
    if (RT_SUCCESS(rc))
    {
        /*
         * Allocate buffer and recode it.
         */
        char *pszResult = (char *)RTMemAlloc(cch + 1);
        if (pszResult)
        {
            rc = rtUtf16RecodeAsUtf8(pwszString, RTSTR_MAX, pszResult, cch, &cch);
            if (RT_SUCCESS(rc))
            {
                *ppszString = pszResult;
                return rc;
            }

            RTMemFree(pszResult);
        }
        else
            rc = VERR_NO_STR_MEMORY;
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTUtf16ToUtf8);


RTDECL(int)  RTUtf16ToUtf8Ex(PCRTUTF16 pwszString, size_t cwcString, char **ppsz, size_t cch, size_t *pcch)
{
    /*
     * Validate input.
     */
    Assert(VALID_PTR(pwszString));
    Assert(VALID_PTR(ppsz));
    Assert(!pcch || VALID_PTR(pcch));

    /*
     * Validate the UTF-16 string and calculate the length of the UTF-8 encoding of it.
     */
    size_t cchResult;
    int rc = rtUtf16CalcUtf8Length(pwszString, cwcString, &cchResult);
    if (RT_SUCCESS(rc))
    {
        if (pcch)
            *pcch = cchResult;

        /*
         * Check buffer size / Allocate buffer and recode it.
         */
        bool fShouldFree;
        char *pszResult;
        if (cch > 0 && *ppsz)
        {
            fShouldFree = false;
            if (RT_UNLIKELY(cch <= cchResult))
                return VERR_BUFFER_OVERFLOW;
            pszResult = *ppsz;
        }
        else
        {
            *ppsz = NULL;
            fShouldFree = true;
            cch = RT_MAX(cch, cchResult + 1);
            pszResult = (char *)RTMemAlloc(cch);
        }
        if (pszResult)
        {
            rc = rtUtf16RecodeAsUtf8(pwszString, cwcString, pszResult, cch - 1, &cch);
            if (RT_SUCCESS(rc))
            {
                *ppsz = pszResult;
                return rc;
            }

            if (fShouldFree)
                RTMemFree(pszResult);
        }
        else
            rc = VERR_NO_STR_MEMORY;
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTUtf16ToUtf8Ex);


RTDECL(size_t) RTUtf16CalcUtf8Len(PCRTUTF16 pwsz)
{
    size_t cch;
    int rc = rtUtf16CalcUtf8Length(pwsz, RTSTR_MAX, &cch);
    return RT_SUCCESS(rc) ? cch : 0;
}
RT_EXPORT_SYMBOL(RTUtf16CalcUtf8Len);


RTDECL(int) RTUtf16CalcUtf8LenEx(PCRTUTF16 pwsz, size_t cwc, size_t *pcch)
{
    size_t cch;
    int rc = rtUtf16CalcUtf8Length(pwsz, cwc, &cch);
    if (pcch)
        *pcch = RT_SUCCESS(rc) ? cch : ~(size_t)0;
    return rc;
}
RT_EXPORT_SYMBOL(RTUtf16CalcUtf8LenEx);


RTDECL(RTUNICP) RTUtf16GetCpInternal(PCRTUTF16 pwsz)
{
    const RTUTF16 wc = *pwsz;

    /* simple */
    if (wc < 0xd800 || (wc > 0xdfff && wc < 0xfffe))
        return wc;
    if (wc < 0xfffe)
    {
        /* surrogate pair */
        if (wc < 0xdc00)
        {
            const RTUTF16 wc2 = pwsz[1];
            if (wc2 >= 0xdc00 && wc2 <= 0xdfff)
            {
                RTUNICP uc = 0x10000 + (((wc & 0x3ff) << 10) | (wc2 & 0x3ff));
                return uc;
            }

            RTStrAssertMsgFailed(("wc=%#08x wc2=%#08x - invalid 2nd char in surrogate pair\n", wc, wc2));
        }
        else
            RTStrAssertMsgFailed(("wc=%#08x - invalid surrogate pair order\n", wc));
    }
    else
        RTStrAssertMsgFailed(("wc=%#08x - endian indicator\n", wc));
    return RTUNICP_INVALID;
}
RT_EXPORT_SYMBOL(RTUtf16GetCpInternal);


RTDECL(int) RTUtf16GetCpExInternal(PCRTUTF16 *ppwsz, PRTUNICP pCp)
{
    const RTUTF16 wc = **ppwsz;

    /* simple */
    if (wc < 0xd800 || (wc > 0xdfff && wc < 0xfffe))
    {
        (*ppwsz)++;
        *pCp = wc;
        return VINF_SUCCESS;
    }

    int rc;
    if (wc < 0xfffe)
    {
        /* surrogate pair */
        if (wc < 0xdc00)
        {
            const RTUTF16 wc2 = (*ppwsz)[1];
            if (wc2 >= 0xdc00 && wc2 <= 0xdfff)
            {
                RTUNICP uc = 0x10000 + (((wc & 0x3ff) << 10) | (wc2 & 0x3ff));
                *pCp = uc;
                (*ppwsz) += 2;
                return VINF_SUCCESS;
            }

            RTStrAssertMsgFailed(("wc=%#08x wc2=%#08x - invalid 2nd char in surrogate pair\n", wc, wc2));
        }
        else
            RTStrAssertMsgFailed(("wc=%#08x - invalid surrogate pair order\n", wc));
        rc = VERR_INVALID_UTF16_ENCODING;
    }
    else
    {
        RTStrAssertMsgFailed(("wc=%#08x - endian indicator\n", wc));
        rc = VERR_CODE_POINT_ENDIAN_INDICATOR;
    }
    *pCp = RTUNICP_INVALID;
    (*ppwsz)++;
    return rc;
}
RT_EXPORT_SYMBOL(RTUtf16GetCpExInternal);


RTDECL(PRTUTF16) RTUtf16PutCpInternal(PRTUTF16 pwsz, RTUNICP CodePoint)
{
    /* simple */
    if (    CodePoint < 0xd800
        ||  (   CodePoint > 0xdfff
             && CodePoint < 0xfffe))
    {
        *pwsz++ = (RTUTF16)CodePoint;
        return pwsz;
    }

    /* surrogate pair */
    if (CodePoint >= 0x10000 && CodePoint <= 0x0010ffff)
    {
        CodePoint -= 0x10000;
        *pwsz++ = 0xd800 | (CodePoint >> 10);
        *pwsz++ = 0xdc00 | (CodePoint & 0x3ff);
        return pwsz;
    }

    /* invalid code point. */
    RTStrAssertMsgFailed(("Invalid codepoint %#x\n", CodePoint));
    *pwsz++ = 0x7f;
    return pwsz;
}
RT_EXPORT_SYMBOL(RTUtf16PutCpInternal);


/**
 * Validate the UTF-16 encoding and calculates the length of a Latin1 encoding.
 *
 * @returns iprt status code.
 * @param   pwsz        The UTF-16 string.
 * @param   cwc         The max length of the UTF-16 string to consider.
 * @param   pcch        Where to store the length (excluding '\\0') of the Latin1 string. (cch == cb, btw)
 */
static int rtUtf16CalcLatin1Length(PCRTUTF16 pwsz, size_t cwc, size_t *pcch)
{
    int     rc = VINF_SUCCESS;
    size_t  cch = 0;
    while (cwc > 0)
    {
        RTUTF16 wc = *pwsz++; cwc--;
        if (!wc)
            break;
        else if (RT_LIKELY(wc < 0x100))
            ++cch;
        else
        {
            if (wc < 0xd800 || wc > 0xdfff)
            {
                if (wc >= 0xfffe)
                {
                    RTStrAssertMsgFailed(("endian indicator! wc=%#x\n", wc));
                    rc = VERR_CODE_POINT_ENDIAN_INDICATOR;
                    break;
                }
            }
            else
            {
                if (wc >= 0xdc00)
                {
                    RTStrAssertMsgFailed(("Wrong 1st char in surrogate! wc=%#x\n", wc));
                    rc = VERR_INVALID_UTF16_ENCODING;
                    break;
                }
                if (cwc <= 0)
                {
                    RTStrAssertMsgFailed(("Invalid length! wc=%#x\n", wc));
                    rc = VERR_INVALID_UTF16_ENCODING;
                    break;
                }
                wc = *pwsz++; cwc--;
                if (wc < 0xdc00 || wc > 0xdfff)
                {
                    RTStrAssertMsgFailed(("Wrong 2nd char in surrogate! wc=%#x\n", wc));
                    rc = VERR_INVALID_UTF16_ENCODING;
                    break;
                }
            }

            rc = VERR_NO_TRANSLATION;
            break;
        }
    }

    /* done */
    *pcch = cch;
    return rc;
}


/**
 * Recodes an valid UTF-16 string as Latin1.
 *
 * @returns iprt status code.
 * @param   pwsz        The UTF-16 string.
 * @param   cwc         The number of RTUTF16 characters to process from pwsz. The recoding
 *                      will stop when cwc or '\\0' is reached.
 * @param   psz         Where to store the Latin1 string.
 * @param   cch         The size of the Latin1 buffer, excluding the terminator.
 */
static int rtUtf16RecodeAsLatin1(PCRTUTF16 pwsz, size_t cwc, char *psz, size_t cch)
{
    unsigned char  *pch = (unsigned char *)psz;
    int             rc  = VINF_SUCCESS;
    while (cwc > 0)
    {
        RTUTF16 wc = *pwsz++; cwc--;
        if (!wc)
            break;
        if (RT_LIKELY(wc < 0x100))
        {
            if (RT_UNLIKELY(cch < 1))
            {
                RTStrAssertMsgFailed(("Buffer overflow! 1\n"));
                rc = VERR_BUFFER_OVERFLOW;
                break;
            }
            cch--;
            *pch++ = (unsigned char)wc;
        }
        else
        {
            if (wc < 0xd800 || wc > 0xdfff)
            {
                if (wc >= 0xfffe)
                {
                    RTStrAssertMsgFailed(("endian indicator! wc=%#x\n", wc));
                    rc = VERR_CODE_POINT_ENDIAN_INDICATOR;
                    break;
                }
            }
            else
            {
                if (wc >= 0xdc00)
                {
                    RTStrAssertMsgFailed(("Wrong 1st char in surrogate! wc=%#x\n", wc));
                    rc = VERR_INVALID_UTF16_ENCODING;
                    break;
                }
                if (cwc <= 0)
                {
                    RTStrAssertMsgFailed(("Invalid length! wc=%#x\n", wc));
                    rc = VERR_INVALID_UTF16_ENCODING;
                    break;
                }
                RTUTF16 wc2 = *pwsz++; cwc--;
                if (wc2 < 0xdc00 || wc2 > 0xdfff)
                {
                    RTStrAssertMsgFailed(("Wrong 2nd char in surrogate! wc=%#x\n", wc));
                    rc = VERR_INVALID_UTF16_ENCODING;
                    break;
                }
            }

            rc = VERR_NO_TRANSLATION;
            break;
        }
    }

    /* done */
    *pch = '\0';
    return rc;
}


RTDECL(int)  RTUtf16ToLatin1(PCRTUTF16 pwszString, char **ppszString)
{
    /*
     * Validate input.
     */
    Assert(VALID_PTR(ppszString));
    Assert(VALID_PTR(pwszString));
    *ppszString = NULL;

    /*
     * Validate the UTF-16 string and calculate the length of the UTF-8 encoding of it.
     */
    size_t cch;
    int rc = rtUtf16CalcLatin1Length(pwszString, RTSTR_MAX, &cch);
    if (RT_SUCCESS(rc))
    {
        /*
         * Allocate buffer and recode it.
         */
        char *pszResult = (char *)RTMemAlloc(cch + 1);
        if (pszResult)
        {
            rc = rtUtf16RecodeAsLatin1(pwszString, RTSTR_MAX, pszResult, cch);
            if (RT_SUCCESS(rc))
            {
                *ppszString = pszResult;
                return rc;
            }

            RTMemFree(pszResult);
        }
        else
            rc = VERR_NO_STR_MEMORY;
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTUtf16ToLatin1);


RTDECL(int)  RTUtf16ToLatin1Ex(PCRTUTF16 pwszString, size_t cwcString, char **ppsz, size_t cch, size_t *pcch)
{
    /*
     * Validate input.
     */
    AssertPtr(pwszString);
    AssertPtr(ppsz);
    AssertPtrNull(pcch);

    /*
     * Validate the UTF-16 string and calculate the length of the Latin1 encoding of it.
     */
    size_t cchResult;
    int rc = rtUtf16CalcLatin1Length(pwszString, cwcString, &cchResult);
    if (RT_SUCCESS(rc))
    {
        if (pcch)
            *pcch = cchResult;

        /*
         * Check buffer size / Allocate buffer and recode it.
         */
        bool fShouldFree;
        char *pszResult;
        if (cch > 0 && *ppsz)
        {
            fShouldFree = false;
            if (cch <= cchResult)
                return VERR_BUFFER_OVERFLOW;
            pszResult = *ppsz;
        }
        else
        {
            *ppsz = NULL;
            fShouldFree = true;
            cch = RT_MAX(cch, cchResult + 1);
            pszResult = (char *)RTMemAlloc(cch);
        }
        if (pszResult)
        {
            rc = rtUtf16RecodeAsLatin1(pwszString, cwcString, pszResult, cch - 1);
            if (RT_SUCCESS(rc))
            {
                *ppsz = pszResult;
                return rc;
            }

            if (fShouldFree)
                RTMemFree(pszResult);
        }
        else
            rc = VERR_NO_STR_MEMORY;
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTUtf16ToLatin1Ex);


RTDECL(size_t) RTUtf16CalcLatin1Len(PCRTUTF16 pwsz)
{
    size_t cch;
    int rc = rtUtf16CalcLatin1Length(pwsz, RTSTR_MAX, &cch);
    return RT_SUCCESS(rc) ? cch : 0;
}
RT_EXPORT_SYMBOL(RTUtf16CalcLatin1Len);


RTDECL(int) RTUtf16CalcLatin1LenEx(PCRTUTF16 pwsz, size_t cwc, size_t *pcch)
{
    size_t cch;
    int rc = rtUtf16CalcLatin1Length(pwsz, cwc, &cch);
    if (pcch)
        *pcch = RT_SUCCESS(rc) ? cch : ~(size_t)0;
    return rc;
}
RT_EXPORT_SYMBOL(RTUtf16CalcLatin1LenEx);


/**
 * Calculates the UTF-16 length of a Latin1 string.  In fact this is just the
 * original length, but the function saves us nasty comments to that effect
 * all over the place.
 *
 * @returns IPRT status code.
 * @param   psz     Pointer to the Latin1 string.
 * @param   cch     The max length of the string. (btw cch = cb)
 *                  Use RTSTR_MAX if all of the string is to be examined.s
 * @param   pcwc    Where to store the length of the UTF-16 string as a number of RTUTF16 characters.
 */
static int rtLatin1CalcUtf16Length(const char *psz, size_t cch, size_t *pcwc)
{
    *pcwc = RTStrNLen(psz, cch);
    return VINF_SUCCESS;
}


/**
 * Recodes a Latin1 string as UTF-16.  This is just a case of expanding it to
 * sixteen bits, as Unicode is a superset of Latin1.
 *
 * Since we know the input is valid, we do *not* perform length checks.
 *
 * @returns iprt status code.
 * @param   psz     The Latin1 string to recode.
 * @param   cch     The number of chars (the type char, so bytes if you like) to process of the Latin1 string.
 *                  The recoding will stop when cch or '\\0' is reached. Pass RTSTR_MAX to process up to '\\0'.
 * @param   pwsz    Where to store the UTF-16 string.
 * @param   cwc     The number of RTUTF16 items the pwsz buffer can hold, excluding the terminator ('\\0').
 */
static int rtLatin1RecodeAsUtf16(const char *psz, size_t cch, PRTUTF16 pwsz, size_t cwc)
{
    int                     rc   = VINF_SUCCESS;
    const unsigned char    *puch = (const unsigned char *)psz;
    PRTUTF16                pwc  = pwsz;
    while (cch-- > 0)
    {
        /* read the next char and check for terminator. */
        const unsigned char uch = *puch;
        if (!uch)
            break;

        /* check for output overflow */
        if (RT_UNLIKELY(cwc < 1))
        {
            rc = VERR_BUFFER_OVERFLOW;
            break;
        }

        /* expand the code point */
        *pwc++ = uch;
        cwc--;
        puch++;
    }

    /* done */
    *pwc = '\0';
    return rc;
}


RTDECL(int) RTLatin1ToUtf16(const char *pszString, PRTUTF16 *ppwszString)
{
    /*
     * Validate input.
     */
    Assert(VALID_PTR(ppwszString));
    Assert(VALID_PTR(pszString));
    *ppwszString = NULL;

    /*
     * Validate the input and calculate the length of the UTF-16 string.
     */
    size_t cwc;
    int rc = rtLatin1CalcUtf16Length(pszString, RTSTR_MAX, &cwc);
    if (RT_SUCCESS(rc))
    {
        /*
         * Allocate buffer.
         */
        PRTUTF16 pwsz = (PRTUTF16)RTMemAlloc((cwc + 1) * sizeof(RTUTF16));
        if (pwsz)
        {
            /*
             * Encode the UTF-16 string.
             */
            rc = rtLatin1RecodeAsUtf16(pszString, RTSTR_MAX, pwsz, cwc);
            if (RT_SUCCESS(rc))
            {
                *ppwszString = pwsz;
                return rc;
            }
            RTMemFree(pwsz);
        }
        else
            rc = VERR_NO_UTF16_MEMORY;
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTLatin1ToUtf16);


RTDECL(int)  RTLatin1ToUtf16Ex(const char *pszString, size_t cchString, PRTUTF16 *ppwsz, size_t cwc, size_t *pcwc)
{
    /*
     * Validate input.
     */
    Assert(VALID_PTR(pszString));
    Assert(VALID_PTR(ppwsz));
    Assert(!pcwc || VALID_PTR(pcwc));

    /*
     * Validate the input and calculate the length of the UTF-16 string.
     */
    size_t cwcResult;
    int rc = rtLatin1CalcUtf16Length(pszString, cchString, &cwcResult);
    if (RT_SUCCESS(rc))
    {
        if (pcwc)
            *pcwc = cwcResult;

        /*
         * Check buffer size / Allocate buffer.
         */
        bool fShouldFree;
        PRTUTF16 pwszResult;
        if (cwc > 0 && *ppwsz)
        {
            fShouldFree = false;
            if (cwc <= cwcResult)
                return VERR_BUFFER_OVERFLOW;
            pwszResult = *ppwsz;
        }
        else
        {
            *ppwsz = NULL;
            fShouldFree = true;
            cwc = RT_MAX(cwcResult + 1, cwc);
            pwszResult = (PRTUTF16)RTMemAlloc(cwc * sizeof(RTUTF16));
        }
        if (pwszResult)
        {
            /*
             * Encode the UTF-16 string.
             */
            rc = rtLatin1RecodeAsUtf16(pszString, cchString, pwszResult, cwc - 1);
            if (RT_SUCCESS(rc))
            {
                *ppwsz = pwszResult;
                return rc;
            }
            if (fShouldFree)
                RTMemFree(pwszResult);
        }
        else
            rc = VERR_NO_UTF16_MEMORY;
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTLatin1ToUtf16Ex);


RTDECL(size_t) RTLatin1CalcUtf16Len(const char *psz)
{
    size_t cwc;
    int rc = rtLatin1CalcUtf16Length(psz, RTSTR_MAX, &cwc);
    return RT_SUCCESS(rc) ? cwc : 0;
}
RT_EXPORT_SYMBOL(RTLatin1CalcUtf16Len);


RTDECL(int) RTLatin1CalcUtf16LenEx(const char *psz, size_t cch, size_t *pcwc)
{
    size_t cwc;
    int rc = rtLatin1CalcUtf16Length(psz, cch, &cwc);
    if (pcwc)
        *pcwc = RT_SUCCESS(rc) ? cwc : ~(size_t)0;
    return rc;
}
RT_EXPORT_SYMBOL(RTLatin1CalcUtf16LenEx);
