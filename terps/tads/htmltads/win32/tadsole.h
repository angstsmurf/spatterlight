/* $Header: d:/cvsroot/tads/html/win32/tadsole.h,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsole.h - miscellaneous OLE classes
Function
  
Notes
  
Modified
  03/25/98 MJRoberts  - Creation
*/

#ifndef TADSOLE_H
#define TADSOLE_H

#include <Ole2.h>
#include <Windows.h>

/* ------------------------------------------------------------------------ */
/*
 *   OLE IDataObject implementation for a simple text object.  This
 *   implementation is suitable for OLE drag-source operations, but may
 *   not be suitable for other uses, since we only implement the methods
 *   that are required for drag sources.  
 */
class CTadsDataObjText: public IDataObject
{
public:
    CTadsDataObjText(HGLOBAL texthdl, ULONG textlen)
    {
        /* remember the memory containing the text */
        texthdl_ = texthdl;
        textlen_ = textlen;

        /* start out with a reference count of 1 for our creator */
        ole_refcnt_ =  1;
    }

    CTadsDataObjText(const textchar_t *buf, size_t buflen);

    virtual ~CTadsDataObjText();

    /*
     *   IDataObject implementation 
     */

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **obj);
    ULONG STDMETHODCALLTYPE AddRef() { return ++ole_refcnt_; }
    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE GetData(FORMATETC __RPC_FAR *pformatetcIn,
                                      STGMEDIUM __RPC_FAR *pmedium);

    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC __RPC_FAR *pformatetc,
                                          STGMEDIUM __RPC_FAR *pmedium);

    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC __RPC_FAR *pformatetc);

    HRESULT STDMETHODCALLTYPE
        EnumFormatEtc(DWORD dwDirection,
                      IEnumFORMATETC __RPC_FAR *__RPC_FAR *ppenumFormatEtc);

    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(
        FORMATETC __RPC_FAR *, FORMATETC __RPC_FAR *)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE SetData(FORMATETC __RPC_FAR *,
                                      STGMEDIUM __RPC_FAR *,
                                      BOOL)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC __RPC_FAR *,
                                      DWORD, IAdviseSink __RPC_FAR *p,
                                      DWORD __RPC_FAR *)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD)
        { return E_NOTIMPL; }

    HRESULT STDMETHODCALLTYPE
        EnumDAdvise(IEnumSTATDATA __RPC_FAR *__RPC_FAR *)
        { return E_NOTIMPL; }

protected:
    /* copy our text into a given HGLOBAL */
    void copy_text_to_hglobal(HGLOBAL dsthdl);
    
    /* reference count */
    ULONG ole_refcnt_;

    /* handle to our text, and size of the text */
    HGLOBAL texthdl_;
    ULONG textlen_;
};


/* ------------------------------------------------------------------------ */
/*
 *   General-purpose IEnumFORMATETC implementation for a static FORMATETC
 *   list.  
 */

/*
 *   List of FORMATETC's for CGenFmtEtc.  A CGenFmtEtc can create one of
 *   these objects, or can be created based on one of these objects.
 *   These objects can be shared among CGenFmtEtc's; we use a reference
 *   count to keep track of how many CGenFmtEtc's are currently using this
 *   object.  
 */
class CGenFmtEtcList
{
public:
    CGenFmtEtcList(int fmtcnt);
    ~CGenFmtEtcList();

    void addref() { ++refcnt_; }
    void remref();

    ULONG fmtcnt_;
    FORMATETC *fmtetcs_;

private:
    int refcnt_;
};

/*
 *   Simple general-purpose FORMATETC enumerator implementation.  We keep
 *   a static list of FORMATETC's that we can enumerate.  
 */
class CGenFmtEtc: public IEnumFORMATETC
{
public:
    CGenFmtEtc(int fmtcnt);
    CGenFmtEtc(class CGenFmtEtcList *list);

    virtual ~CGenFmtEtc()
    {
        /* we're done with the list - release our reference to it */
        fmtlist_->remref();
    }

    /* set a single FORMATETC item */
    void set_item(int idx, FORMATETC *fmtetc);

    /* set a block of FORMATETC items */
    void set_items(int start_idx, FORMATETC *fmtetcs, int cnt);

    /* query an interface */
    HRESULT STDMETHODCALLTYPE
        QueryInterface(REFIID riid,
                       void __RPC_FAR *__RPC_FAR *ppvObject);

    /* add a reference */
    ULONG STDMETHODCALLTYPE AddRef() { return ++refcnt_; }

    /* release a reference */
    ULONG STDMETHODCALLTYPE Release();

    /* get the next entry or entries */
    HRESULT STDMETHODCALLTYPE Next(ULONG celt,
                                   FORMATETC __RPC_FAR *rgelt,
                                   ULONG __RPC_FAR *pceltFetched);

    /* skip one or more entries */
    HRESULT STDMETHODCALLTYPE Skip(ULONG celt);

    /* reset this interface */
    HRESULT STDMETHODCALLTYPE Reset()
    {
        curpos_ = 0;
        return S_OK;
    }

    /* create a clone of this interface */
    HRESULT STDMETHODCALLTYPE
        Clone(IEnumFORMATETC __RPC_FAR *__RPC_FAR *ppenum);

private:
    ULONG refcnt_;
    ULONG curpos_;
    class CGenFmtEtcList *fmtlist_;
};


#endif /* TADSOLE_H */
