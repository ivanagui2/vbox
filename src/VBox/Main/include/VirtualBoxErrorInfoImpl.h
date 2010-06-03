/** @file
 * VirtualBoxErrorInfo COM class definition.
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

#ifndef ____H_VIRTUALBOXERRORINFOIMPL
#define ____H_VIRTUALBOXERRORINFOIMPL

#include "VirtualBoxBase.h"

using namespace com;

class ATL_NO_VTABLE VirtualBoxErrorInfo
    : public CComObjectRootEx<CComMultiThreadModel>
    , VBOX_SCRIPTABLE_IMPL(IVirtualBoxErrorInfo)
#ifndef VBOX_WITH_XPCOM /* IErrorInfo doesn't inherit from IDispatch, ugly 3am hack: */
    , public IDispatch
#endif
{
public:

    DECLARE_NOT_AGGREGATABLE(VirtualBoxErrorInfo)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VirtualBoxErrorInfo)
        COM_INTERFACE_ENTRY(IErrorInfo)
        COM_INTERFACE_ENTRY(IVirtualBoxErrorInfo)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

#ifndef VBOX_WITH_XPCOM

    HRESULT init(IErrorInfo *aInfo);

    STDMETHOD(GetGUID)(GUID *guid);
    STDMETHOD(GetSource)(BSTR *source);
    STDMETHOD(GetDescription)(BSTR *description);
    STDMETHOD(GetHelpFile)(BSTR *pBstrHelpFile);
    STDMETHOD(GetHelpContext)(DWORD *pdwHelpContext);

    // IDispatch forwarding - 3am hack.
    typedef IDispatchImpl<IVirtualBoxErrorInfo, &IID_IVirtualBoxErrorInfo, &LIBID_VirtualBox, kTypeLibraryMajorVersion, kTypeLibraryMinorVersion> idi;

    STDMETHOD(GetTypeInfoCount)(UINT *pcInfo)
    {
        return idi::GetTypeInfoCount(pcInfo);
    }

    STDMETHOD(GetTypeInfo)(UINT iInfo, LCID Lcid, ITypeInfo **ppTypeInfo)
    {
        return idi::GetTypeInfo(iInfo, Lcid, ppTypeInfo);
    }

    STDMETHOD(GetIDsOfNames)(REFIID rIID, LPOLESTR *papwszNames, UINT cNames, LCID Lcid, DISPID *paDispIDs)
    {
        return idi::GetIDsOfNames(rIID, papwszNames, cNames, Lcid, paDispIDs);
    }

    STDMETHOD(Invoke)(DISPID idDispMember, REFIID rIID, LCID Lcid, WORD fw, DISPPARAMS *pDispParams,
                      VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *piErrArg)
    {
        return idi::Invoke(idDispMember, rIID, Lcid, fw, pDispParams, pVarResult, pExcepInfo, piErrArg);
    }

#else // defined(VBOX_WITH_XPCOM)

    HRESULT init(nsIException *aInfo);

    NS_DECL_NSIEXCEPTION

#endif

    VirtualBoxErrorInfo() : mResultCode(S_OK) {}

    // public initializer/uninitializer for internal purposes only
    HRESULT init(HRESULT aResultCode, const GUID &aIID,
                 CBSTR aComponent, CBSTR aText,
                 IVirtualBoxErrorInfo *aNext = NULL);

    // IVirtualBoxErrorInfo properties
    STDMETHOD(COMGETTER(ResultCode))(LONG *aResultCode);
    STDMETHOD(COMGETTER(InterfaceID))(BSTR *aIID);
    STDMETHOD(COMGETTER(Component))(BSTR *aComponent);
    STDMETHOD(COMGETTER(Text))(BSTR *aText);
    STDMETHOD(COMGETTER(Next))(IVirtualBoxErrorInfo **aNext);

private:
    // FIXME: declare these here until VBoxSupportsTranslation base
    //        is available in this class.
    static const char *tr(const char *a) { return a; }
    static HRESULT setError(HRESULT rc,
                            const char * /* a */,
                            const char * /* b */,
                            void *       /* c */) { return rc; }

    HRESULT mResultCode;
    Bstr mText;
    Guid mIID;
    Bstr mComponent;
    ComPtr<IVirtualBoxErrorInfo> mNext;
};

#endif // !____H_VIRTUALBOXERRORINFOIMPL

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
