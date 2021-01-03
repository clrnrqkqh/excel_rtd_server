
#pragma once
#pragma warning (disable:28251)
///
/// \author Andrew Wang
///
/// \brief: contains the declaration of a simpl real-time-data server for excel
///
/// \class: AwRTD
///
#include "comdef.h"
#include "IRTDServer_h.h"
#include "datacache.h"

#include <string>
#include <thread>
#pragma comment(lib, "advapi32.lib")

extern LONG g_cOb;	//global count of the number of objects created.

struct INonDelegatingUnknown
{
   /***** INonDelegatingUnknown Methods *****/
   virtual STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, 
      void ** ppvObj) = 0;      
   virtual STDMETHODIMP_(ULONG) NonDelegatingAddRef() = 0;
   virtual STDMETHODIMP_(ULONG) NonDelegatingRelease() = 0;
};

class AwRTD : public INonDelegatingUnknown,
					public IRtdServer
{
    /*
    struct MyParam
    {
        IStream* stream;
        AwRTD* server;
    };
    */
private:
   int m_refCount;
   IUnknown* m_pOuterUnknown;
   ITypeInfo* m_pTypeInfoInterface;
   // HANDLE m_hthread = INVALID_HANDLE_VALUE;
   DataCache m_cache;
   std::thread m_thread;
   
public:
    void OnThreadProc(IStream* stream);
    /*
    static void ThreadProc(LPVOID lpParam)
    {
        struct MyParam* myparam = (MyParam*)lpParam;
        myparam->server->OnThreadProc(myparam->stream);
    }
    void OnTestThread();
    */
public:
   //Constructor
   AwRTD(IUnknown* pUnkOuter);
   //Destructor
   ~AwRTD();
   
   STDMETHODIMP LoadTypeInfo(ITypeInfo** pptinfo, REFCLSID clsid,
      LCID lcid);

   /***** IUnknown Methods *****/
   STDMETHODIMP QueryInterface(REFIID riid, void ** ppvObj);
   STDMETHODIMP_(ULONG) AddRef();
   STDMETHODIMP_(ULONG) Release();
   
   /***** INonDelegatingUnknown Methods *****/
   STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, 
      void ** ppvObj);      
   STDMETHODIMP_(ULONG) NonDelegatingAddRef();
   STDMETHODIMP_(ULONG) NonDelegatingRelease();
   
   /***** IDispatch Methods *****/
   STDMETHODIMP GetTypeInfoCount(UINT *iTInfo);
   STDMETHODIMP GetTypeInfo(UINT iTInfo, LCID lcid, 
      ITypeInfo **ppTInfo);
   STDMETHODIMP GetIDsOfNames(REFIID riid,  
      OLECHAR **rgszNames, 
      UINT cNames,  LCID lcid,
      DISPID *rgDispId);
   STDMETHODIMP Invoke(DISPID dispIdMember, REFIID riid, LCID lcid,
      WORD wFlags, DISPPARAMS* pDispParams,
      VARIANT* pVarResult, EXCEPINFO* pExcepInfo,
      UINT* puArgErr);
   
   /***** IVCRTDServer Methods *****/
   STDMETHODIMP ServerStart( 
      IRTDUpdateEvent *CallbackObject,
      long *pfRes);
   
   STDMETHODIMP ConnectData( 
      long TopicID,
      SAFEARRAY * *Strings,
      VARIANT_BOOL *GetNewValues,
      VARIANT *pvarOut);
   
   STDMETHODIMP RefreshData( 
      long *TopicCount,
      SAFEARRAY * *parrayOut);
   
   STDMETHODIMP DisconnectData( 
      long TopicID);
   
   STDMETHODIMP Heartbeat( 
      long *pfRes);
   
   STDMETHODIMP ServerTerminate( void);   
};
