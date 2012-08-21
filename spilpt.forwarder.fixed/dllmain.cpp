// dllmain.cpp : Definiert den Einstiegspunkt f�r die DLL-Anwendung.
#define BUILD_DLL
#include "dllmain.h"

//CONFIGURATION
#define DEBUG_LEVEL_ALL		0
#define DEBUG_LEVEL_CONFIG	1
#define DEBUG_LEVEL_ERROR	2

#define DEBUG_LEVEL DEBUG_LEVEL_ALL
#define DEBUG_OUT	false
//END CONFIGURATION


HINSTANCE g_hInstance=0;
HMODULE g_hForward=0;
bool g_bDebug=false;
int g_iDebugLevel = 0;
TCHAR* g_slibName = 0;

typedef int (__cdecl *type_spifns_init)(); //Return 0 on no error, negative on error
typedef void (__cdecl *type_spifns_close)();
typedef void (__cdecl *type_spifns_getvarlist)(SPIVARDEF **ppList, unsigned int *pnCount);
typedef const char * (__cdecl *type_spifns_getvar)(const char *szName);
typedef int (__cdecl *type_spifns_get_version)(); //Should return 259
typedef void (__cdecl *type_spifns_enumerate_ports)(spifns_enumerate_ports_callback pCallback, void *pData);
typedef void (__cdecl *type_spifns_chip_select)(unsigned int nUnknown);
typedef const char* (__cdecl *type_spifns_command)(const char *szCmd); //Return 0 on no error, or string on error.
typedef unsigned int (__cdecl *type_spifns_get_last_error)(unsigned short *pnErrorAddress, const char **szErrorString); //Returns where the error occured, or 0x100 for none
typedef int (__cdecl *type_spifns_bluecore_xap_stopped)(); //Returns -1 on error, 0 on XAP running, 1 on stopped
typedef int (__cdecl *type_spifns_sequence)(SPISEQ *pSequence, unsigned int nCount); //Return 0 on no error
typedef void (__cdecl *type_spifns_set_debug_callback)(spifns_debug_callback pCallback);

type_spifns_init forward_spifns_init=0;
type_spifns_close forward_spifns_close=0;
type_spifns_getvarlist forward_spifns_getvarlist=0;
type_spifns_getvar forward_spifns_getvar=0;
type_spifns_get_version forward_spifns_get_version=0;
type_spifns_enumerate_ports forward_spifns_enumerate_ports=0;
type_spifns_chip_select forward_spifns_chip_select=0;
type_spifns_command forward_spifns_command=0;
type_spifns_get_last_error forward_spifns_get_last_error=0;
type_spifns_bluecore_xap_stopped forward_spifns_bluecore_xap_stopped=0;
type_spifns_sequence forward_spifns_sequence=0;
type_spifns_set_debug_callback forward_spifns_set_debug_callback=0;

BOOL WINAPI DllMain(HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved) {
	CIniReader *iniReader;
	TCHAR defaultLibName[255] = TEXT("spilpt_src");
	TCHAR* libName = 0;

	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:{
		//if (g_hInstance != 0)
			//return FALSE;
		g_hInstance=hinstDll;

		iniReader = new CIniReader(TEXT(".\\spilpt.ini"));
		g_bDebug = iniReader->ReadBoolean(TEXT("Forwarder"),TEXT("Debug"),g_bDebug);
		g_iDebugLevel = iniReader->ReadInteger(TEXT("Forwarder"),TEXT("DebugLevel"),DEBUG_LEVEL);
		g_slibName = iniReader->ReadString(TEXT("Forwarder"),TEXT("DLL"),defaultLibName);
		
		//Write to Console even if it is a GUI application
		//=> Ctrl-C might be needed to return to console after application exits
		if (g_bDebug && GetConsoleWindow()==0)
		{
			AttachConsole(ATTACH_PARENT_PROCESS);
			FILE *console;
			freopen_s(&console, "CONOUT$", "w", stdout);
			//End: Write to console init
		}

		if (g_bDebug)
			_tprintf(TEXT("Debug Mode ON, Level: %i\n"), g_iDebugLevel);

		_tprintf(TEXT("spilpt.forwarded will load lib: %s\n"),g_slibName);
		}
		break;
	case DLL_PROCESS_DETACH:{
		if (g_bDebug)
			_tprintf(TEXT("spilpt.forwarded unloading\n"));
		FreeLibrary(g_hForward);
		//g_hInstance=0;
		}
		break;
	}
	return TRUE;
}

void __cdecl load_forwarder_library() {
	g_hForward=LoadLibrary(g_slibName);
	
	if (!g_hForward) {
		_tprintf(TEXT("spilpt.forwarded could not load '%s' (spilpt.ini \\ [Forwarder] \\ DLL) Error: %i\n"),g_slibName, GetLastError());
		return;
		// FALSE;
	}
	delete g_slibName;

	if (g_bDebug)
		_tprintf(TEXT("spilpt.forwarded loaded\n"));

	forward_spifns_init=(type_spifns_init)GetProcAddress(g_hForward,"spifns_init");
	forward_spifns_close=(type_spifns_close)GetProcAddress(g_hForward,"spifns_close");
	forward_spifns_getvarlist=(type_spifns_getvarlist)GetProcAddress(g_hForward,"spifns_getvarlist");
	forward_spifns_getvar=(type_spifns_getvar)GetProcAddress(g_hForward,"spifns_getvar");
	forward_spifns_get_version=(type_spifns_get_version)GetProcAddress(g_hForward,"spifns_get_version");
	forward_spifns_enumerate_ports=(type_spifns_enumerate_ports)GetProcAddress(g_hForward,"spifns_enumerate_ports");
	forward_spifns_chip_select=(type_spifns_chip_select)GetProcAddress(g_hForward,"spifns_chip_select");
	forward_spifns_command=(type_spifns_command)GetProcAddress(g_hForward,"spifns_command");
	forward_spifns_get_last_error=(type_spifns_get_last_error)GetProcAddress(g_hForward,"spifns_get_last_error");
	forward_spifns_bluecore_xap_stopped=(type_spifns_bluecore_xap_stopped)GetProcAddress(g_hForward,"spifns_bluecore_xap_stopped");
	forward_spifns_sequence=(type_spifns_sequence)GetProcAddress(g_hForward,"spifns_sequence");
	forward_spifns_set_debug_callback=(type_spifns_set_debug_callback)GetProcAddress(g_hForward,"spifns_set_debug_callback");

}

int __cdecl spifns_init() {
	if (!g_hForward)
		load_forwarder_library();

	int nRetval=-1;
	if (forward_spifns_init)
		nRetval=forward_spifns_init();
	if (g_bDebug && g_iDebugLevel <= DEBUG_LEVEL_ALL)
		_tprintf(TEXT("spifns_init(); returned %d\n"),nRetval);
	return nRetval;
}
void __cdecl spifns_close() {
	if (!g_hForward)
		load_forwarder_library();

	if (g_bDebug && g_iDebugLevel <= DEBUG_LEVEL_ALL)
		_tprintf(TEXT("spifns_close();\n"));
	if (forward_spifns_close)
		forward_spifns_close();
}
void __cdecl spifns_getvarlist(SPIVARDEF **ppList, unsigned int *pnCount) {
	if (!g_hForward)
		load_forwarder_library();

	if (ppList)
		*ppList=0;
	if (*pnCount)
		*pnCount=0;
	if (forward_spifns_getvarlist)
		forward_spifns_getvarlist(ppList,pnCount);
	if (g_bDebug && g_iDebugLevel <= DEBUG_LEVEL_CONFIG) {
		_tprintf(TEXT("spifns_getvarlist(&pList,&nCount); returned %d entries:\n"),pnCount?*pnCount:0);
		if (ppList && *ppList && pnCount) {
			for (unsigned int i=0; i<*pnCount; i++)
				_tprintf(TEXT("\t\"%hs\", \"%hs\", %d\n"),(*ppList)[i].szName,(*ppList)[i].szDefault,(*ppList)[i].nUnknown);
		} else {
			_tprintf(TEXT("\tNo list returned\n"));
		}
	}
}
const char * __cdecl spifns_getvar(const char *szName) {
	if (!g_hForward)
		load_forwarder_library();

	const char *szRetval=0;
	if (forward_spifns_getvar)
		szRetval=forward_spifns_getvar(szName);
	if (g_bDebug && g_iDebugLevel <= DEBUG_LEVEL_CONFIG)
		_tprintf(TEXT("spifns_getvar(\"%hs\"); returned \"%hs\"\n"),szName?szName:"(null)",szRetval?szRetval:"(null)");
	return szRetval;
}
int __cdecl spifns_get_version() {
	if (!g_hForward)
		load_forwarder_library();

	int nRetval=0;
	if (forward_spifns_get_version)
		nRetval=forward_spifns_get_version();
	if (g_bDebug && g_iDebugLevel <= DEBUG_LEVEL_ALL)
		_tprintf(TEXT("spifns_get_version(); returned %d\n"),nRetval);
	return nRetval;
}
struct spifns_port_enumerator_data {
	spifns_enumerate_ports_callback pForward;
	void *pForwardData;
};
void __cdecl spifns_port_enumerator(unsigned int nPort, const char *szPortName, void *pData) {
	if (!g_hForward)
		load_forwarder_library();

	if (g_bDebug && g_iDebugLevel <= DEBUG_LEVEL_ALL)
		_tprintf(TEXT("\tPort: %d \"%hs\"\n"),nPort,szPortName?szPortName:"");
	spifns_port_enumerator_data *pCastData=(spifns_port_enumerator_data *)pData;
	pCastData->pForward(nPort,szPortName,pCastData->pForwardData);
}
void __cdecl spifns_enumerate_ports(spifns_enumerate_ports_callback pCallback, void *pData) {
	spifns_port_enumerator_data d;
	d.pForward=pCallback;
	d.pForwardData=pData;
	if (g_bDebug && g_iDebugLevel <= DEBUG_LEVEL_CONFIG)
		_tprintf(TEXT("spifns_enumerate_ports(0x%08X,0x%08X);\n"),pCallback,pData);
	if (forward_spifns_enumerate_ports)
		forward_spifns_enumerate_ports(spifns_port_enumerator,(void*)&d);
}
void __cdecl spifns_chip_select(unsigned int nUnknown) {
	if (g_bDebug && g_iDebugLevel <= DEBUG_LEVEL_CONFIG)
		_tprintf(TEXT("spifns_chip_select(%d);\n"),nUnknown);
	if (forward_spifns_chip_select)
		forward_spifns_chip_select(nUnknown);
}
const char* __cdecl spifns_command(const char *szCmd) {
	const char *szRetval=0;
	if (forward_spifns_command)
		szRetval=forward_spifns_command(szCmd);
	if (g_bDebug && g_iDebugLevel <= DEBUG_LEVEL_CONFIG)
		_tprintf(TEXT("spifns_command(\"%hs\"); returned \"%hs\"\n"),szCmd?szCmd:"(null)",szRetval?szRetval:"(null)");
	return szRetval;
}
unsigned int __cdecl spifns_get_last_error(unsigned short *pnErrorAddress, const char **szErrorString) {
	unsigned int nError=SPIERR_NO_ERROR;
	if (forward_spifns_get_last_error)
		nError=forward_spifns_get_last_error(pnErrorAddress,szErrorString);
	if (g_bDebug && ((g_iDebugLevel <= DEBUG_LEVEL_ERROR && nError != SPIERR_NO_ERROR) || (g_iDebugLevel <= DEBUG_LEVEL_ALL))) {
		char szErrorCode[1024];
		switch (nError) {
			case SPIERR_NO_ERROR: strcpy_s<1024>(szErrorCode,"SPIERR_NO_ERROR"); break;
			case SPIERR_MALLOC_FAILED: strcpy_s<1024>(szErrorCode,"SPIERR_MALLOC_FAILED"); break;
			case SPIERR_NO_LPT_PORT_SELECTED: strcpy_s<1024>(szErrorCode,"SPIERR_NO_LPT_PORT_SELECTED"); break;
			case SPIERR_READ_FAILED: strcpy_s<1024>(szErrorCode,"SPIERR_READ_FAILED"); break;
			case SPIERR_IOCTL_FAILED: strcpy_s<1024>(szErrorCode,"SPIERR_IOCTL_FAILED"); break;
			default: sprintf_s<1024>(szErrorCode,"0x%08X",nError); break;
		}
		_tprintf(TEXT("spifns_get_last_error(...,...); returned %hs, nAddress=0x%04X, \"%hs\"\n"),szErrorCode,pnErrorAddress?*pnErrorAddress:0,(szErrorString && *szErrorString)?*szErrorString:"(null)");
	}
	return nError;
}
int __cdecl spifns_bluecore_xap_stopped() {
	int nRetval=-1;
	if (forward_spifns_bluecore_xap_stopped)
		nRetval=forward_spifns_bluecore_xap_stopped();
	if (g_bDebug && g_iDebugLevel <= DEBUG_LEVEL_CONFIG)
		_tprintf(TEXT("spifns_bluecore_xap_stopped(); returned %d\n"),nRetval);
	return nRetval;
}
int __cdecl spifns_sequence(SPISEQ *pSequence, unsigned int nCount) {
	int nRetval=1;
	if (g_bDebug) {
		_tprintf(TEXT("spifns_sequence({\n"));
		for (unsigned int i=0; i<nCount; i++) {
			switch (pSequence[i].nType) {
			case SPISEQ::TYPE_SETVAR:
				if(g_iDebugLevel <= DEBUG_LEVEL_CONFIG)
					_tprintf(TEXT("\t{TYPE_SETVAR,\"%hs\",\"%hs\"},\n"),pSequence[i].setvar.szName?pSequence[i].setvar.szName:"(null)",pSequence[i].setvar.szValue?pSequence[i].setvar.szValue:"(null)");
				break;
			case SPISEQ::TYPE_READ:
				if(g_iDebugLevel <= DEBUG_LEVEL_ALL)
					_tprintf(TEXT("\t{TYPE_READ,0x%04X,0x%04X},\n"),pSequence[i].rw.nAddress,pSequence[i].rw.nLength);
				break;
			case SPISEQ::TYPE_WRITE:{
				if(g_iDebugLevel <= DEBUG_LEVEL_ALL)
				{
					_tprintf(TEXT("\t{TYPE_WRITE,0x%04X,0x%04X,{"),pSequence[i].rw.nAddress,pSequence[i].rw.nLength);
					if (pSequence[i].rw.pnData==NULL) {
						_tprintf(TEXT("NULL}},\n"));
					} else {
						for (unsigned int j=0; j<pSequence[i].rw.nLength; j++) {	//Changed from j to i, Nippey
							if ((j%8)==0)
								_tprintf(TEXT("\n\t\t"));
							_tprintf(TEXT("0x%04X "),pSequence[i].rw.pnData[j]);
						}
						_tprintf(TEXT("\n\t} },\n"));
					}
				}	
									}break;
			default:
				if(g_iDebugLevel <= DEBUG_LEVEL_ALL)
					_tprintf(TEXT("\t{%d}\n"),pSequence[i].nType);
			}
		}
		_tprintf(TEXT("},%d);\n"),nCount);
	}
	if (forward_spifns_sequence)
		nRetval=forward_spifns_sequence(pSequence,nCount);
	if (g_bDebug) {
		_tprintf(TEXT("return value: %d\n"),nRetval);
		if (nRetval==0) {
			for (unsigned int i=0; i<nCount; i++) {
				switch (pSequence[i].nType) {
				case SPISEQ::TYPE_READ:{
					if(g_iDebugLevel <= DEBUG_LEVEL_ALL)
					{
						_tprintf(TEXT("\tRead result %d:"),i);
						if (pSequence[i].rw.pnData) {
							for (unsigned int j=0; j<pSequence[i].rw.nLength; j++) {
								if ((j%8)==0)
									_tprintf(TEXT("\n\t\t"));
								_tprintf(TEXT("0x%04X "),pSequence[i].rw.pnData[j]);
							}
							_tprintf(TEXT("\n"));
						} else {
							_tprintf(TEXT(" NULL\n"));
						}
					}
									   }break;
				}
			}
		}
	}
	return nRetval;
}


spifns_debug_callback pRealDebugCallback=0;
void __cdecl spifns_hooked_debug_callback(const char *szDebug) {
	if (g_bDebug && g_iDebugLevel <= DEBUG_LEVEL_ALL)
		_tprintf(TEXT("spifns_debug_callback(\"%hs\");\n"),szDebug);
	if (pRealDebugCallback)
		pRealDebugCallback(szDebug);
}
void __cdecl spifns_set_debug_callback(spifns_debug_callback pCallback) {
	pRealDebugCallback=pCallback;
	if (forward_spifns_set_debug_callback)
		forward_spifns_set_debug_callback(pCallback?spifns_hooked_debug_callback:0);
}