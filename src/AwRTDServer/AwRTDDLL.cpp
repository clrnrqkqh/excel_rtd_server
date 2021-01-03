///
/// \author Andrew Wang
///
/// \brief: contains the class factory and standard DLL
///         functions for the AwRTD COM object. It is fairly
///         generic for use with other COM objects
///
/// \class: AwRTDClassFactory
///
#include "configuration.h"
#include "AwRTDDLL.h"
#include "AwRTDImpl.h"

#pragma warning (disable:4267)

// {C28ABB27-5E5F-4B7E-9A1B-F4DF70C23479}
static const GUID CLSID_AwRTDServer =
{ 0xc28abb27, 0x5e5f, 0x4b7e, { 0x9a, 0x1b, 0xf4, 0xdf, 0x70, 0xc2, 0x34, 0x79 } };

AwRTDClassFactory::AwRTDClassFactory()
{
   m_refCount = 0;
   // load configuration
   Configuration::instance().init();
   aw::logger::init("", Configuration::instance().getLogDir(), true); // this is first RTD to be called
   aw::logger::log("AwRTDClassFactory::AwRTDClassFactory()");
}

AwRTDClassFactory::~AwRTDClassFactory()
{
    aw::logger::log("AwRTDClassFactory::~AwRTDClassFactory()");
}

/******************************************************************************
*   IUnknown Interfaces -- All COM objects must implement, either directly or 
*   indirectly, the IUnknown interface.
******************************************************************************/

/******************************************************************************
*   QueryInterface -- Determines if this component supports the requested 
*   interface, places a pointer to that interface in ppvObj if it's available,
*   and returns S_OK.  If not, sets ppvObj to NULL and returns E_NOINTERFACE.
******************************************************************************/
STDMETHODIMP AwRTDClassFactory::QueryInterface(REFIID riid, void ** ppvObj)
{
   if (riid == IID_IUnknown){
      *ppvObj = static_cast<IClassFactory*>(this);
   }
   
   else if (riid == IID_IClassFactory){
       *ppvObj = static_cast<IClassFactory*>(this);
   }
   
   else{
      *ppvObj = NULL;
      return E_NOINTERFACE;
   }
   
   static_cast<IUnknown*>(*ppvObj)->AddRef();
   return S_OK;
}

/******************************************************************************
*   AddRef() -- In order to allow an object to delete itself when it is no 
*   longer needed, it is necessary to maintain a count of all references to 
*   this object.  When a new reference is created, this function increments
*   the count.
******************************************************************************/
STDMETHODIMP_(ULONG) AwRTDClassFactory::AddRef()
{
   //tracing purposes only
   //LogMessage("AwRTDSecmasterv2ClassFactory::AddRef");
   
   return ++m_refCount;
}

/******************************************************************************
*   Release() -- When a reference to this object is removed, this function 
*   decrements the reference count.  If the reference count is 0, then this 
*   function deletes this object and returns 0;
******************************************************************************/
STDMETHODIMP_(ULONG) AwRTDClassFactory::Release()
{
   //tracing purposes only
//   LogMessage("AwRTDSecmasterv2ClassFactory::Release");
   
   if (--m_refCount == 0)
   {
      delete this;
      return 0;
   }
   return m_refCount;
}


/******* IClassFactory Methods *******/
/******************************************************************************
*	CreateInstance() -- This method attempts to create an instance of VCRTDServer
*	and returns it to the caller.  It maintains a count of the number of
*	created objects.
******************************************************************************/
STDMETHODIMP AwRTDClassFactory::CreateInstance(LPUNKNOWN pUnkOuter, 
                                                      REFIID riid, 
                                                      LPVOID *ppvObj)
{
   //tracing purposes only
   //LogMessage("PriceRTDServerClassFactory::CreateInstance");

   HRESULT hr;
   AwRTD* pObj;
   
   *ppvObj = NULL;
   hr = ResultFromScode(E_OUTOFMEMORY);

   //It's illegal to ask for anything but IUnknown when aggregating
   if ((pUnkOuter != NULL) && (riid != IID_IUnknown))
      return E_INVALIDARG;
   
   //Create a new instance of VCRTDServer
   pObj = new AwRTD(pUnkOuter);
   
   if (pObj == NULL)
      return hr;
   
   //Return the resulting object
   hr = pObj->NonDelegatingQueryInterface(riid, ppvObj);

   if (FAILED(hr))
      delete pObj;
   
   return hr;
}

/******************************************************************************
*	LockServer() -- This method maintains a count of the current locks on this
*	DLL.  The count is used to determine if the DLL can be unloaded, or if
*	clients are still using it.
******************************************************************************/
STDMETHODIMP AwRTDClassFactory::LockServer(BOOL fLock)
{
   //tracing purposes only
//   LogMessage("PriceRTDServerClassFactory::LockServer");

   if (fLock)
      InterlockedIncrement( &g_cLock );
   else
      InterlockedDecrement( &g_cLock );
   return NOERROR;
}

/******* Exported DLL functions *******/
/******************************************************************************
*  g_RegTable -- This N*3 array contains the keys, value names, and values that
*  are associated with this dll in the registry.
******************************************************************************/
const char *g_RegTable[][3] = {
   //format is {key, value name, value }
   {"AwRTDServer.RTDFunctions", 0, "AwRTDServer.RTDFunctions"},
   {"AwRTDServer.RTDFunctions\\CLSID", 0, "{C28ABB27-5E5F-4B7E-9A1B-F4DF70C23479}"},
   
   {"CLSID\\{C28ABB27-5E5F-4B7E-9A1B-F4DF70C23479}", 0, "AwRTDServer.RTDFunctions"},
   {"CLSID\\{C28ABB27-5E5F-4B7E-9A1B-F4DF70C23479}\\InprocServer32", 0, 
      (const char*)-1},
   {"CLSID\\{C28ABB27-5E5F-4B7E-9A1B-F4DF70C23479}\\ProgId", 0, 
   "AwRTDServer.RTDFunctions"},
   {"CLSID\\{C28ABB27-5E5F-4B7E-9A1B-F4DF70C23479}\\TypeLib", 0, 
   "{28712CBB-30E8-43DF-9079-06C2B1C2F33D}"},

   //	copied this from Kruglinski with my uuids and names.  
   //Just marks where the typelib is
   {"TypeLib\\{28712CBB-30E8-43DF-9079-06C2B1C2F33D}", 0, "AwRTDServer.RTDFunctions"},
   {"TypeLib\\{28712CBB-30E8-43DF-9079-06C2B1C2F33D}\\1.0", 0, 
   "AwRTDServer.RTDFunctions"},
   {"TypeLib\\{28712CBB-30E8-43DF-9079-06C2B1C2F33D}\\1.0\\0", 0, "win32"},
   {"TypeLib\\{28712CBB-30E8-43DF-9079-06C2B1C2F33D}\\1.0\\0\\Win32", 0, 
      (const char*)-1},
   {"TypeLib\\{28712CBB-30E8-43DF-9079-06C2B1C2F33D}\\1.0\\FLAGS", 0, "0"},
};
	

/******************************************************************************
*  DLLRegisterServer -- This method is the exported method that is used by
*  COM to self-register this component.  It removes the need for a .reg file.
*  ( Taken from Don Box's _Essential COM_ pg. 110-112)
******************************************************************************/
STDAPI DllRegisterServer(void)
{
   HRESULT hr = S_OK;
   
   //look up server's file name
   char szFileName[255];
   HMODULE dllModule = GetModuleHandle("AwRTDServer.dll");
   GetModuleFileName(dllModule, szFileName, 255);
   
   //the typelib should be in the same directory
   char szTypeLibName[255];
   char* pszFileName = NULL;
   memset( szTypeLibName, '\0', 255);
   lstrcpy( szTypeLibName, szFileName );
   pszFileName = strstr( szTypeLibName, "AwRTDServer.dll");

   //register entries from the table
   int nEntries = sizeof(g_RegTable)/sizeof(*g_RegTable);
   for (int i = 0; SUCCEEDED(hr) && i < nEntries; i++)
   {
      const char *pszName = g_RegTable[i][0];
      const char *pszValueName = g_RegTable[i][1];
      const char *pszValue = g_RegTable[i][2];
      
      //Map rogue values to module file name
      if (pszValue == (const char*)-1)
         pszValue = szFileName;
      
      //Create the key
      HKEY hkey;
      long err = RegCreateKeyA( HKEY_CLASSES_ROOT, pszName, &hkey);
      
      //Set the value
      if (err == ERROR_SUCCESS){
         err = RegSetValueExA( hkey, pszValueName, 0, REG_SZ, 
            (const BYTE*)pszValue, (strlen(pszValue) + 1));
         RegCloseKey(hkey);
      }
      
      //if cannot add key or value, back out and fail
      if (err != ERROR_SUCCESS){
         DllUnregisterServer();
         hr = SELFREG_E_CLASS;
      }
   }
   return hr;
}

/******************************************************************************
*  DllUnregisterServer -- This method is the exported method that is used by 
*  COM to remove the keys added to the registry by DllRegisterServer.  It
*  is essentially for housekeeping.
*  (Taken from Don Box, _Essential COM_ pg 112)
******************************************************************************/
STDAPI DllUnregisterServer(void)
{
   HRESULT hr = S_OK;
   
   int nEntries = sizeof(g_RegTable)/sizeof(*g_RegTable);
   
   for (int i = nEntries - 1; i >= 0; i--){
      const char * pszKeyName = g_RegTable[i][0];
      
      long err = RegDeleteKeyA(HKEY_CLASSES_ROOT, pszKeyName);
      if (err != ERROR_SUCCESS)
         hr = S_FALSE;
   }
   return hr;
}

/******************************************************************************
*	DllGetClassObject() -- This method is the exported method that clients use
*	to create objects in the DLL.  It uses class factories to generate the
*	desired object and returns it to the caller.  The caller must call Release()
*	on the object when they're through with it.
******************************************************************************/
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID FAR * ppvObj)
{
   //tracing purposes only
   //LogMessage("DLLGetClassObject");

   //Make sure the requested class is supported by this server
   if (!IsEqualCLSID(rclsid, CLSID_AwRTDServer))
      return ResultFromScode(E_FAIL);
   
   //Make sure the requested interface is supported
   if ((!IsEqualCLSID(riid, IID_IUnknown)) && (!IsEqualCLSID(riid, 
      IID_IClassFactory)))
      return ResultFromScode(E_NOINTERFACE);
   
   //Create the class factory
   *ppvObj = (LPVOID) new AwRTDClassFactory();
   
   //error checking
   if (*ppvObj == NULL)
      return ResultFromScode(E_OUTOFMEMORY);
   
   //Addref the Class Factory
   ((LPUNKNOWN)*ppvObj)->AddRef();
   
   return NOERROR;
}

/******************************************************************************
*	DllCanUnloadNow() -- This method checks to see if it's alright to unload 
*	the dll by determining if there are currently any locks on the dll.
******************************************************************************/
STDAPI DllCanUnloadNow()
{
   if ((g_cLock == 0) && (g_cOb == 0))
      return S_OK;
   else
      return S_FALSE;
}