#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/html/win32/tadsole.cpp,v 1.2 1999/05/17 02:52:25 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  tadsole.cpp - miscellaneous TADS OLE classes
Function
  
Notes
  
Modified
  03/25/98 MJRoberts  - Creation
*/

#include <Ole2.h>
#include <Windows.h>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSOLE_H
#include "tadsole.h"
#endif

CTadsDataObjText::CTadsDataObjText(const textchar_t *buf, size_t buflen)
{
    textchar_t *ptr;
    
    /* create a new global memory handle to hold the data */
    texthdl_ = GlobalAlloc(GHND, buflen + sizeof(textchar_t));

    /* lock the handle */
    ptr = (textchar_t *)GlobalLock(texthdl_);

    /* copy the data */
    memcpy(ptr, buf, buflen);

    /* add a null terminator */
    ptr[buflen] = '\0';

    /* done with the handle - unlock it */
    GlobalUnlock(texthdl_);

    /* remember the length */
    textlen_ = buflen + sizeof(textchar_t);

    /* start out with a reference count of 1 for our creator */
    ole_refcnt_ =  1;
}

CTadsDataObjText::~CTadsDataObjText()
{
    /* we're done with the text handle */
    GlobalFree(texthdl_);
}

/*
 *   get an interface 
 */
HRESULT STDMETHODCALLTYPE
   CTadsDataObjText::QueryInterface(REFIID iid, void **obj)
{
    /* see if we provide the interface */
    if (iid == IID_IUnknown)
        *obj = (void *)(IUnknown *)this;
    else if (iid == IID_IDataObject)
        *obj = (void *)(IDataObject *)this;
    else
        return E_NOINTERFACE;

    /* add a new reference on behalf of the caller */
    AddRef();

    /* success */
    return S_OK;
}

/*
 *   release a reference on the interface 
 */
ULONG STDMETHODCALLTYPE CTadsDataObjText::Release()
{
    ULONG ret;

    /* decrement the reference count and note the new value for return */
    ret = --ole_refcnt_;

    /* if we've dropped to zero, delete me */
    if (ole_refcnt_ == 0)
        delete this;

    /* return the new reference count */
    return ret;
}

/*
 *   render data in a given format 
 */
HRESULT STDMETHODCALLTYPE
   CTadsDataObjText::GetData(FORMATETC __RPC_FAR *pformatetcIn,
                             STGMEDIUM __RPC_FAR *pmedium)
{
    HRESULT res;
    
    /* make sure we can render to this format */
    if ((res = QueryGetData(pformatetcIn)) != S_OK)
        return res;

    /* return data as an HGLOBAL */
    pmedium->tymed = TYMED_HGLOBAL;

    /* allocate the new handle */
    pmedium->hGlobal = GlobalAlloc(GMEM_SHARE, textlen_);

    /* load data our into the hglobal */
    copy_text_to_hglobal(pmedium->hGlobal);

    /* caller is to release using standard mechanism (i.e., GlobalFree) */
    pmedium->pUnkForRelease = 0;

    /* success */
    return S_OK;
}

/*
 *   render data into a given storage medium 
 */
HRESULT STDMETHODCALLTYPE
   CTadsDataObjText::GetDataHere(FORMATETC __RPC_FAR *pformatetc,
                                 STGMEDIUM __RPC_FAR *pmedium)
{
    HRESULT res;
    
    /* make sure we can render into the given medium */
    if ((res = QueryGetData(pformatetc)) != S_OK)
        return res;

    /* check the storage medium type */
    switch(pmedium->tymed)
    {
    case TYMED_HGLOBAL:
        /* make sure we have room in the caller's HGLOBAL */
        if (GlobalSize(pmedium->hGlobal) < textlen_)
            return STG_E_MEDIUMFULL;

        /* load our data into their hglobal */
        copy_text_to_hglobal(pmedium->hGlobal);
        break;

    default:
        /* we can't render to other media */
        return DV_E_TYMED;
    }

    /* the caller must release the medium */
    pmedium->pUnkForRelease = 0;

    /* success */
    return S_OK;
}

/*
 *   Copy our text into a given hglobal 
 */
void CTadsDataObjText::copy_text_to_hglobal(HGLOBAL dsthdl)
{
    void *dstptr;
    void *srcptr;
    
    /* lock both handles */
    dstptr = GlobalLock(dsthdl);
    srcptr = GlobalLock(texthdl_);

    /* copy the text */
    memcpy(dstptr, srcptr, textlen_);

    /* unlock both handles */
    GlobalUnlock(dstptr);
    GlobalUnlock(srcptr);
}

/*
 *   determine if we can render data in the given format 
 */
HRESULT STDMETHODCALLTYPE
   CTadsDataObjText::QueryGetData(FORMATETC __RPC_FAR *fmtetc)
{
    /* make sure all of the fields are valid */
    if (fmtetc->cfFormat != CF_TEXT
        || fmtetc->ptd != 0)
        return DV_E_FORMATETC;
    if ((fmtetc->dwAspect & DVASPECT_CONTENT) == 0)
        return DV_E_DVASPECT;
    if (fmtetc->lindex != -1)
        return DV_E_LINDEX;
    if ((fmtetc->tymed & TYMED_HGLOBAL) == 0)
        return DV_E_TYMED;

    /* everything looks good */
    return S_OK;
}

/*
 *   get an enumerator for our supported formats 
 */
HRESULT STDMETHODCALLTYPE CTadsDataObjText::EnumFormatEtc(
    DWORD /*dwDirection*/,
    IEnumFORMATETC __RPC_FAR *__RPC_FAR *ppenumFormatEtc)
{
    CGenFmtEtc *new_enum;
    FORMATETC fmtetc;
    
    /* create a new general-purpose enumerator */
    new_enum = new CGenFmtEtc(1);

    /* add a text format to its list */
    fmtetc.cfFormat = CF_TEXT;
    fmtetc.ptd = 0;
    fmtetc.dwAspect = DVASPECT_CONTENT;
    fmtetc.lindex = -1;
    fmtetc.tymed = TYMED_HGLOBAL;
    new_enum->set_item(0, &fmtetc);

    /* return the enumerator */
    *ppenumFormatEtc = new_enum;
    return S_OK;
}

/* ------------------------------------------------------------------------ */
/*
 *   CGenFmtEtcList implementation 
 */

CGenFmtEtcList::CGenFmtEtcList(int fmtcnt)
{
    /* start out referenced by the creator */
    refcnt_ = 1;
    
    /* 
     *   remember how many structures we have, and allocate space for them 
     */
    fmtcnt_ = (ULONG)fmtcnt;
    fmtetcs_ = (FORMATETC *)th_malloc(fmtcnt * sizeof(FORMATETC));
}

CGenFmtEtcList::~CGenFmtEtcList()
{
    /* free the array of FORMATETC structures */
    th_free(fmtetcs_);
}

void CGenFmtEtcList::remref()
{
    /* remove a reference; delete myself if this leaves me unreferenced */
    if (--refcnt_ == 0)
        delete this;
}

/* ------------------------------------------------------------------------ */
/*
 *   CGenFmtEtc implementation 
 */

CGenFmtEtc::CGenFmtEtc(int fmtcnt)
{
    /* start out with one reference (for our creator) */
    refcnt_ = 1;

    /* start out at first element */
    curpos_ = 0;

    /* allocate our new list */
    fmtlist_ = new CGenFmtEtcList(fmtcnt);
}

CGenFmtEtc::CGenFmtEtc(CGenFmtEtcList *list)
{
    /* remember the list */
    fmtlist_ = list;

    /* add our reference to it */
    fmtlist_->addref();
}

/*
 *   set a single item in the FORMATETC list 
 */
void CGenFmtEtc::set_item(int idx, FORMATETC *fmtetc)
{
    /* copy the item */
    memcpy(&fmtlist_->fmtetcs_[idx], fmtetc, sizeof(FORMATETC));
}

/*
 *   set a block of items in the FORMATETC list 
 */
void CGenFmtEtc::set_items(int start_idx, FORMATETC *fmtetcs, int cnt)
{
    /* copy the block of items */
    memcpy(&fmtlist_->fmtetcs_[start_idx], fmtetcs,
           cnt * sizeof(FORMATETC));
}

/*
 *   query an interface 
 */
HRESULT STDMETHODCALLTYPE
   CGenFmtEtc::QueryInterface(REFIID riid,
                              void __RPC_FAR *__RPC_FAR *ppvObject)
{
    /* determine if I provide the interface */
    if (riid == IID_IUnknown)
        *ppvObject = (void *)(IUnknown *)this;
    else if (riid == IID_IEnumFORMATETC)
        *ppvObject = (void *)(IUnknown *)this;
    else
    {
        /* I don't provide the interface - return failure */
        return E_NOINTERFACE;
    }

    /* add a reference and return success */
    AddRef();
    return S_OK;
}

/*
 *   release a reference 
 */
ULONG STDMETHODCALLTYPE CGenFmtEtc::Release()
{
    ULONG ret;
    
    /* decrement the count and remember the new value for return */
    ret = --refcnt_;
    
    /* if we have no references, delete myself */
    if (refcnt_ == 0)
        delete this;
    
    /* return the new reference count */
    return ret;
}


/*
 *   get the next set of entries 
 */
HRESULT STDMETHODCALLTYPE
   CGenFmtEtc::Next(ULONG celt, FORMATETC __RPC_FAR *rgelt,
                    ULONG __RPC_FAR *pceltFetched)
{
    HRESULT ret;

    /* limit the number we copy to the number we have remaining */
    if (fmtlist_->fmtcnt_ - curpos_ < celt)
    {
        /* we can't give them as many as they asked for */
        celt = fmtlist_->fmtcnt_ - curpos_;
        ret = S_FALSE;
    }
    else
    {
        /* we can retrieve everything they asked for */
        ret = S_OK;
    }
    
    /* tell them how many we're returning, if they want to know */
    if (pceltFetched != 0)
        *pceltFetched = celt;
    
    /* copy the values */
    if (celt != 0)
        memcpy(rgelt, &fmtlist_->fmtetcs_[curpos_],
               celt * sizeof(FORMATETC));
    
    /* move past the ones we copied */
    curpos_ += celt;
    
    /* return the result */
    return ret;
}

/*
 *   skip one or more entries 
 */
HRESULT STDMETHODCALLTYPE CGenFmtEtc::Skip(ULONG celt)
{
    /* make sure we don't go past the end of the list */
    if (curpos_ + celt > fmtlist_->fmtcnt_)
    {
        /* going too far - go to the end of the list */
        curpos_ = fmtlist_->fmtcnt_;
        return S_FALSE;
    }
    else
    {
        /* move ahead by the desired amount */
        curpos_ += celt;
        return S_OK;
    }
}

/*
 *   create a clone of this interface 
 */
HRESULT STDMETHODCALLTYPE
   CGenFmtEtc::Clone(IEnumFORMATETC __RPC_FAR *__RPC_FAR *ppenum)
{
    CGenFmtEtc *newfmt;
    
    /* create a new enumerator that points to our FORMATETC list */
    newfmt = new CGenFmtEtc(fmtlist_);

    /* set the new interface at the same position in the enumeration */
    newfmt->curpos_ = curpos_;

    /* success */
    *ppenum = newfmt;
    return S_OK;
}

