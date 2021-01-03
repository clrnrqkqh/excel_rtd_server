///
/// \author Andrew Wang
///
/// \brief: contains the declaration of a simpl real-time-data server for excel
///
/// \class: AwRTD
///
/// Microsoft.Office...Excel.Option.RTDThrottleInterval 0
///
#include "datacache.h"
#include "AwRTDImpl.h"
#include "configuration.h"

#include <iostream>
#include <algorithm>

// RTD required
LONG g_cOb = 0;	//global count of the number of objects created.

AwRTD::AwRTD(IUnknown* pUnkOuter)
{
	m_refCount = 0;
	m_pTypeInfoInterface = NULL;
   
	AW_LOG("AwRTD::AwRTD");
	//Get the TypeInfo for this object
	HRESULT hr = LoadTypeInfo(&m_pTypeInfoInterface, IID_IRtdServer, 0x0);
	AW_LOG("LoadTypeInfo: S_OK: " << (hr == S_OK));
	//Set up the aggregation
	if (pUnkOuter != NULL)
		m_pOuterUnknown = pUnkOuter;
	else
		m_pOuterUnknown = reinterpret_cast<IUnknown*>(static_cast<INonDelegatingUnknown*>(this));   

	//Increment the object count so the server knows not to unload
	InterlockedIncrement( &g_cOb );
	AW_LOG("AwRTD::AwRTD: END");
}
AwRTD::~AwRTD()
{
	AW_LOG("~AwRTD::AwRTD");
	//Clean up the type information
	if (m_pTypeInfoInterface != NULL) {
		m_pTypeInfoInterface->Release();
		m_pTypeInfoInterface = NULL;
	}

	//Decrement the object count
	InterlockedDecrement( &g_cOb );
}

void AwRTD::OnThreadProc(IStream* stream)
{
	AW_LOG("OnThreadProc");
	CoInitialize(NULL);
	HRESULT hr = S_OK;
	IRTDUpdateEvent* pRTDUpdate = NULL;

	//Retrieve the RTDUpdate object
	hr = CoGetInterfaceAndReleaseStream((IStream*)stream, IID_IRTDUpdateEvent, (void**)&pRTDUpdate);

	//Set the heartbeat interval to a little more than our timer interval
	if (pRTDUpdate != NULL) {
		pRTDUpdate->AddRef();
		hr = pRTDUpdate->put_HeartbeatInterval(1200);

		HANDLE handles[2] = { Configuration::instance().getShutdownHandle(), Configuration::instance().getNotifyHandle() };
		DWORD dwIdx;
		bool done = false;
		uint64_t secondCount = 0;
		while (!done)
		{
			dwIdx = WaitForMultipleObjects(2, handles, FALSE, 1000);
			switch (dwIdx)
			{
			case WAIT_OBJECT_0:
				AW_LOG("Got shutdown event");
				return; // this thread will not exit peacefully
				pRTDUpdate->Disconnect();
				done = true;
				break;
			case WAIT_OBJECT_0 + 1:
				pRTDUpdate->UpdateNotify(); // don't call update from datacache is because this is required from m_thread
				break;
			case WAIT_TIMEOUT:
				if (secondCount++ % 10 == 0)
					Configuration::instance().readVerbose();
				break;
			default:
				AW_LOG("wrong return for WaitForMultipleObjects");
				break;
			}
		}
		//Clean up the RTDUpdate object
		pRTDUpdate->Release();
	}
	CoUninitialize();
	//All done...
}

/******************************************************************************
*  ServerStart -- The ServerStart method is called immediately after a 
*  real-time data server is instantiated.
*  Parameters: CallbackObject -- interface pointer the PriceRTDServer uses to 
*                                indicate new data is available.
*              pfRes -- set to positive value to indicate success.  0 or 
*                       negative value indicates failure.
*  Returns: S_OK
*           E_POINTER
*           E_FAIL
******************************************************************************/
STDMETHODIMP AwRTD::ServerStart(IRTDUpdateEvent *CallbackObject,
										long *pfRes)
{
	AW_LOG("AwRTD::ServerStart");
	HRESULT hr = S_OK;

	//Check the arguments first
	if ((CallbackObject == NULL) || (pfRes == NULL))
		hr = E_POINTER;
	//Try to launch the data thread
	else {
		// create data thread passing pMarshalStream
		//Marshal the interface to the new thread
		IStream* pMarshalStream = NULL;
		hr = CoMarshalInterThreadInterfaceInStream( IID_IRTDUpdateEvent, 
													CallbackObject, 
													&pMarshalStream );
		
		AW_LOG("Data thread created.");
		/*
		MyParam* param = new struct MyParam;
		param->server = this;
		param->stream = pMarshalStream;
		DWORD dwId(0);
		m_hthread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadProc, param, 0, &dwId);
		*pfRes = dwId;
		* */
		*pfRes = 10;
		m_thread = std::thread([=] {OnThreadProc(pMarshalStream); });
		m_thread.detach(); // tells OS this thread is unjoinable since we know the thread won't exist peacefully.
		// ServerTerminate can't join this thread
		auto rt = m_cache.start();
		AW_LOG("Data Cache started: " << rt);
	}
	return hr;
}

/******************************************************************************
*  ConnectData -- Adds new topics from a real-time data server. The ConnectData
*  method is called when a file is opened that contains real-time data 
*  functions or when a user types in a new formula which contains the RTD 
*  function.
*  Parameters: TopicID -- value assigned by Excel to identify the topic
*              Strings -- safe array containing the strings identifying the 
*                         data to be served.
*              GetNewValues -- BOOLEAN indicating whether to retrieve new 
*                              values or not.
*              pvarOut -- initial value of the topic
*  Returns: S_OK
*           E_POINTER
*           E_FAIL
*  ex: quote, MSFT, bid
******************************************************************************/
STDMETHODIMP AwRTD::ConnectData(long TopicID,
	SAFEARRAY** Strings,
	VARIANT_BOOL* GetNewValues,
	VARIANT* pvarOut)
{
	if (Configuration::instance().getVerbose())
		AW_LOG("AwRTD::ConnectData");

	if (pvarOut == NULL)
		return E_POINTER;

	HRESULT hr = S_OK;
	// dump all params
	LONG lstart, lend;
	if (FAILED(SafeArrayGetLBound(*Strings, 1, &lstart)) || FAILED(SafeArrayGetUBound(*Strings, 1, &lend)))
	{
		AW_LOG("AwRTD::ConnectData: get lbound/ubound failed");
		return hr;
	}
	if (lstart != 0 && lend != 1)
	{
		AW_LOG("AwRTD::ConnectData: lstart not 0 and lend not 1");
		return hr;
	}

	VARIANT var1;
	VariantInit(&var1);

	LONG first(0);

	if (FAILED(SafeArrayGetElement(*Strings, &first, &var1)))
	{
		AW_LOG("AwRTD::ConnectData: couldn't get first parameter (action)");
		return hr;
	}
	std::string action = (const char*)_bstr_t(var1.bstrVal);
	// if not quote, return hr
	if (action.compare(Configuration::command))
	{
		AW_LOG("AwRTD::ConnectData: action<" << action << "> not quote (unsupported)");
		return hr;
	}

	VARIANT var2, var3;
	VariantInit(&var2);
	VariantInit(&var3);
	LONG second(1);
	LONG third(2);
	if (FAILED(SafeArrayGetElement(*Strings, &second, &var2)) ||
		FAILED(SafeArrayGetElement(*Strings, &third, &var3)))
	{
		AW_LOG("AwRTD::ConnectData: couldn't get second parameter (symbol) and third (topic)");
		return hr;
	}
	std::string symbol = (const char*)_bstr_t(var2.bstrVal);
	std::string topic = (const char*)_bstr_t(var3.bstrVal);
	if (Configuration::instance().getVerbose())
		AW_LOG("AwRTD::ConnectData: command<" << action << "> symbol<" << symbol << "> topic<" << topic << "> topic_id<" << TopicID << ">");

	m_cache.add(symbol, topic, TopicID);

	// return stuff
	*GetNewValues = TRUE;
	VariantInit(pvarOut);
	pvarOut->vt = VT_BSTR;
	std::string status = "WAITING";
	pvarOut->bstrVal = SysAllocString(_bstr_t(status.c_str()));
	return hr;
}

/******************************************************************************
*  RefreshData -- This method is called by Microsoft Excel to get new data. 
*  This method call only takes place after being notified by the real-time 
*  data server that there is new data.
*  Parameters: TopicCount -- filled with the count of topics in the safearray
*              parrayOut -- two-dimensional safearray.  First dimension 
*                           contains the list of topic IDs.  Second dimension 
*                           contains the values of those topics.
*  Returns: S_OK
*           E_POINTER
*           E_FAIL
******************************************************************************/
STDMETHODIMP AwRTD::RefreshData( long *TopicCount,
										SAFEARRAY **parrayOut)
{
	AW_LOG("AwRTD::RefreshData");
	HRESULT hr = S_OK;

	//Check the arguments first
	if ((TopicCount == NULL) || (parrayOut == NULL) || (*parrayOut != NULL)) {
		hr = E_POINTER;
		return hr;
	}

	*TopicCount = 0;

	// pulls updated data from m_cache
	std::vector<std::pair<VARIANT, VARIANT>> data;
	m_cache.get(data);
	AW_LOG("AwRTD::RefreshData: get: " << data.size());
	if (data.empty())
		return hr;

	*TopicCount = static_cast<long>(data.size());
	SAFEARRAYBOUND bounds[2];
	long index[2];
	bounds[0].cElements = 2;
	bounds[0].lLbound = 0;
	bounds[1].cElements = *TopicCount;
	bounds[1].lLbound = 0;
	*parrayOut = SafeArrayCreate(VT_VARIANT, 2, bounds);

	int32_t i = 0;
	for (const auto& it : data)
	{
		// puts in topic id
		index[0] = 0;
		index[1] = i;
		SafeArrayPutElement(*parrayOut, index, (void*)(&(it.first)));
		// puts in value
		index[0] = 1;
		index[1] = i;
		SafeArrayPutElement(*parrayOut, index, (void*)(&(it.second)));

		AW_LOG("refresh: topicId=" << it.first.intVal << ", type=" << it.first.vt << ", dat=" << it.second.dblVal << ", type=" << it.second.vt);
		i++;
	}

	return hr;
}

/******************************************************************************
*  DisconnectData -- Notifies the RTD server application that a topic is no 
*  longer in use.
*  Parameters: TopicID -- the topic that is no longer in use.
*  Returns:
******************************************************************************/
STDMETHODIMP AwRTD::DisconnectData( long TopicID)
{
	AW_LOG("AwRTD::DisconnectData");
	HRESULT hr = S_OK;
	return hr;
}

/******************************************************************************
*  Heartbeat -- Determines if the real-time data server is still active.
*  Parameters: pfRes -- filled with zero or negative number to indicate 
*                       failure; positive number indicates success.
*  Returns: S_OK
*           E_POINTER
*           E_FAIL
******************************************************************************/
STDMETHODIMP AwRTD::Heartbeat(long *pfRes)
{
	HRESULT hr = S_OK;

	//Let's reply with the ID of the data thread
	if (pfRes == NULL)
		hr = E_POINTER;
	else
		*pfRes = 1;
	return hr;
}

/******************************************************************************
*  ServerTerminate -- Terminates the connection to the real-time data server.
*  Parameters: none
*  Returns: S_OK
*           E_FAIL
******************************************************************************/
STDMETHODIMP AwRTD::ServerTerminate( void)
{
	AW_LOG("AwRTD::ServerTerminate");
	// don't call m_thread.join() since we can't kill it from here
	m_cache.stop();
	aw::logger::stop();
	return S_OK;
}


///
/// all followings are standard COM required, don't pay too much attention
///
/******************************************************************************
*   LoadTypeInfo -- Gets the type information of an object's interface from the 
*   type library.  Returns S_OK if successful.
******************************************************************************/
STDMETHODIMP AwRTD::LoadTypeInfo(ITypeInfo** pptinfo, 
										REFCLSID clsid, 
										LCID lcid)
{
	HRESULT hr;
	LPTYPELIB ptlib = NULL;
	LPTYPEINFO ptinfo = NULL;
	*pptinfo = NULL;

  // First try to load the type info from a registered type library
	hr = LoadRegTypeLib(LIBID_RTDServerLib, 1, 0, lcid, &ptlib);
	if (FAILED(hr)) {
		//if the libary is not registered, try loading from a file
		hr = LoadTypeLib(L"AwRTDServer.dll", &ptlib);
		if (FAILED(hr)){
			return hr;
		}
	}
   
	// Get type information for interface of the object.
	hr = ptlib->GetTypeInfoOfGuid(clsid, &ptinfo);
	if (FAILED(hr))
	{
		ptlib->Release();
		return hr;
	}
	ptlib->Release();
	*pptinfo = ptinfo;
	return S_OK;
}

/******************************************************************************
*   IUnknown Interfaces -- All COM objects must implement, either 
*  directly or indirectly, the IUnknown interface.
******************************************************************************/

/******************************************************************************
*  QueryInterface -- Determines if this component supports the 
*  requested interface, places a pointer to that interface in ppvObj if it's 
*  available, and returns S_OK.  If not, sets ppvObj to NULL and returns 
*  E_NOINTERFACE.
******************************************************************************/
STDMETHODIMP AwRTD::QueryInterface(REFIID riid, void ** ppvObj)
{
	//defer to the outer unknown
	return m_pOuterUnknown->QueryInterface( riid, ppvObj );
}

/******************************************************************************
*  AddRef() -- In order to allow an object to delete itself when 
*  it is no longer needed, it is necessary to maintain a count of all 
*  references to this object.  When a new reference is created, this function 
*  increments the count.
******************************************************************************/
STDMETHODIMP_(ULONG) AwRTD::AddRef()
{
	//defer to the outer unknown
	return m_pOuterUnknown->AddRef();
}

/******************************************************************************
*  Release() -- When a reference to this object is removed, this 
*  function decrements the reference count.  If the reference count is 0, then 
*  this function deletes this object and returns 0.
******************************************************************************/
STDMETHODIMP_(ULONG) AwRTD::Release()
{
	//defer to the outer unknown
	return m_pOuterUnknown->Release();
}

/******************************************************************************
*   INonDelegatingUnknown Interfaces -- All COM objects must implement, either 
*  directly or indirectly, the IUnknown interface.
******************************************************************************/

/******************************************************************************
*  NonDelegatingQueryInterface -- Determines if this component supports the 
*  requested interface, places a pointer to that interface in ppvObj if it's 
*  available, and returns S_OK.  If not, sets ppvObj to NULL and returns 
*  E_NOINTERFACE.
******************************************************************************/
STDMETHODIMP AwRTD::NonDelegatingQueryInterface(REFIID riid, 
														void ** ppvObj)
{
	if (riid == IID_IUnknown) {
		std::cout << "AwRTD::NonDelegatingQueryInterface->IUnknown" << std::endl;
		*ppvObj = static_cast<INonDelegatingUnknown*>(this);
	}
	else if (riid == IID_IDispatch) {
		*ppvObj = static_cast<IDispatch*>(this);
	}
	else if (riid == IID_IRtdServer) {
		*ppvObj = static_cast<IRtdServer*>(this);
	}
	else {
		LPOLESTR clsidString = NULL;
		StringFromCLSID( riid, &clsidString );
		std::cout << "AwRTD::NonDelegatingQueryInterface->Unsupported Interface -- " << clsidString << std::endl;
		*ppvObj = NULL;
		return E_NOINTERFACE;
	}
   
	static_cast<IUnknown*>(*ppvObj)->AddRef();
	return S_OK;
}

/******************************************************************************
*  NonDelegatingAddRef() -- In order to allow an object to delete itself when 
*  it is no longer needed, it is necessary to maintain a count of all 
*  references to this object.  When a new reference is created, this function 
*  increments the count.
******************************************************************************/
STDMETHODIMP_(ULONG) AwRTD::NonDelegatingAddRef()
{
	return ++m_refCount;
}

/******************************************************************************
*  NonDelegatingRelease() -- When a reference to this object is removed, this 
*  function decrements the reference count.  If the reference count is 0, then 
*  this function deletes this object and returns 0.
******************************************************************************/
STDMETHODIMP_(ULONG) AwRTD::NonDelegatingRelease()
{
	if (m_refCount == 0)
	{
		delete this;
		return 0;
	}
	return m_refCount;
}

/******************************************************************************
*   IDispatch Interface -- This interface allows this class to be used as an
*   automation server, allowing its functions to be called by other COM
*   objects
******************************************************************************/

/******************************************************************************
*   GetTypeInfoCount -- This function determines if the class supports type 
*   information interfaces or not.  It places 1 in iTInfo if the class supports
*   type information and 0 if it doesn't.
******************************************************************************/
STDMETHODIMP AwRTD::GetTypeInfoCount(UINT *iTInfo)
{
	*iTInfo = 0;
	return S_OK;
}

/******************************************************************************
*   GetTypeInfo -- Returns the type information for the class.  For classes 
*   that don't support type information, this function returns E_NOTIMPL;
******************************************************************************/
STDMETHODIMP AwRTD::GetTypeInfo(UINT iTInfo, 
										LCID lcid, 
										ITypeInfo **ppTInfo)
{
	return E_NOTIMPL;
}

/******************************************************************************
*   GetIDsOfNames -- Takes an array of strings and returns an array of DISPID's
*   which corespond to the methods or properties indicated.  If the name is not 
*   recognized, returns DISP_E_UNKNOWNNAME.
******************************************************************************/
STDMETHODIMP AwRTD::GetIDsOfNames(REFIID riid,  
										OLECHAR **rgszNames, 
										UINT cNames,  
										LCID lcid,
										DISPID *rgDispId)
{
	HRESULT hr = E_FAIL;
   
	//Validate arguments
	if (riid != IID_NULL)
		return E_INVALIDARG;
   
	//this API call gets the DISPID's from the type information
	if (m_pTypeInfoInterface != NULL)
		hr = m_pTypeInfoInterface->GetIDsOfNames(rgszNames, cNames, rgDispId);
   
	//DispGetIDsOfNames may have failed, so pass back its return value.
	return hr;
}

/******************************************************************************
*   Invoke -- Takes a dispid and uses it to call another of this class's 
*   methods.  Returns S_OK if the call was successful.
******************************************************************************/
STDMETHODIMP AwRTD::Invoke(DISPID dispIdMember, 
								REFIID riid, 
								LCID lcid,
								WORD wFlags, 
								DISPPARAMS* pDispParams,
								VARIANT* pVarResult, 
								EXCEPINFO* pExcepInfo,
								UINT* puArgErr)
{
	HRESULT hr = DISP_E_PARAMNOTFOUND;
   
	//Validate arguments
	if ((riid != IID_NULL))
		return E_INVALIDARG;

	hr = m_pTypeInfoInterface->Invoke((IRtdServer*)this, 
									dispIdMember, 
									wFlags, 
									pDispParams, 
									pVarResult, 
									pExcepInfo, 
									puArgErr);     

	return S_OK;
}
