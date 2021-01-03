#pragma once
#pragma warning (disable:28251)
///
/// \author Andrew Wang
///
/// \brief: contains the class factory and standard DLL
///         functions for the AwRTD COM object. It is fairly
///         generic for use with other COM objects
///
/// \class: AwRTDClassFactory
/// 
#include "comdef.h"
#include "initguid.h"

class AwRTDClassFactory : public IClassFactory
{
protected:
	ULONG m_refCount;	//reference count

public:
	AwRTDClassFactory();
	~AwRTDClassFactory();

	/******* IUnknown Methods *******/
	STDMETHODIMP QueryInterface(REFIID riid, LPVOID* ppvObj);
	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP_(ULONG) Release();

	/******* IClassFactory Methods *******/
	STDMETHODIMP CreateInstance(LPUNKNOWN, REFIID, LPVOID *);
	STDMETHODIMP LockServer(BOOL);
};

LONG g_cLock = 0;	//global count of the locks on this DLL
typedef AwRTDClassFactory FAR *LPAwRTDClassFactory;

STDAPI DllRegisterServer(void);
STDAPI DllUnregisterServer(void);
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID FAR * ppvObj);
STDAPI DllCanUnloadNow();