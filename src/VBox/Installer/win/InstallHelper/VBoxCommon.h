/* $Id: */
/** @file
* VBoxCommon - Misc helper routines for install helper.
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

#ifndef __VBoxInstallHelper_Common_h__
#define __VBoxInstallHelper_Common_h__

#if (_MSC_VER < 1400) /* Provide _stprintf_s to VC < 8.0. */
int _stprintf_s(TCHAR *buffer, size_t cbBuffer, const TCHAR *format, ...);
#endif

UINT VBoxGetProperty(MSIHANDLE a_hModule, TCHAR* a_pszName, TCHAR* a_pValue, DWORD a_dwSize);
UINT VBoxSetProperty(MSIHANDLE a_hModule, TCHAR* a_pszName, TCHAR* a_pValue);

#endif /* __VBoxInstallHelper_Common_h__ */
