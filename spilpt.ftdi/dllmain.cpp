#define BUILD_DLL
#include "dllmain.h"


//CONFIGURATION
#define BV_MOSI 0x01		//Pin 0 "TXD"
#define BV_MISO 0x02		//Pin 1 "RXD" - INPUT
#define BV_CLK	0x04		//Pin 2 "RTS"
#define BV_MUL	0x08		//Pin 3 "CTS"
#define BV_CSB	0x10		//Pin 4 "DTR"
#define BV_ALLOUT (BV_MOSI | BV_CLK | BV_MUL | BV_CSB)	//These are all output pins

#define FTDI_FREQ (9000)	//Frequency of FTDI, MIN: 3000

#define DEBUG_OUT	false	//Standard value, will be overwritten by value in INI-file
//END CONFIGURATION


HINSTANCE g_hInstance = 0;
bool g_bDebug = false;
int g_iDebugLevel = 0;

DWORD g_NumDevs;
FT_DEVICE_LIST_INFO_NODE *g_DevInfo;

//LPT2FTDI DONE
BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	CIniReader *iniReader;

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		//TEST: Only allow one process to attach!
		//if (g_hInstance != 0)
		//	return FALSE;
		//END TEST

		g_hInstance=hModule;
		
		iniReader = new CIniReader(TEXT(".\\spilpt.ini"));
		g_bDebug = iniReader->ReadBoolean(TEXT("General"),TEXT("Debug"),DEBUG_OUT);
			
		if (g_bDebug && GetConsoleWindow()==0)
		{
			//Write to Console event if it is a GUI application
			//=> Ctrl-C might be needed to return to console after application exits
			AttachConsole(ATTACH_PARENT_PROCESS);
			FILE *console;
			freopen_s(&console, "CONOUT$", "w", stdout);
			//End: Write to console init
		}

		if (g_bDebug)
			printf("> FTDI-DLL loaded, scanning for devices...\n");
		
		FT_STATUS ftStatus;
		ftStatus = FT_CreateDeviceInfoList(&g_NumDevs);
		g_DevInfo = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*g_NumDevs);
		ftStatus = FT_GetDeviceInfoList(g_DevInfo,&g_NumDevs);

		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		g_hInstance=0;
		free(g_DevInfo);
		break;
	}
	return TRUE;
}

//DEFINITIONS
const SPIVARDEF g_pVarList[]={
	{"SPIPORT","0",1},
	{"SPIMUL","0",0},
	{"SPISHIFTPERIOD","0",0},
	{"SPICLOCK","0",0},
	{"SPICMDBITS","0",0},
	{"SPICMDREADBITS","0",0},
	{"SPICMDWRITEBITS","0",0},
	{"SPIMAXCLOCK","1000",0}
};

#define VARLIST_SPISPORT 0
#define VARLIST_SPIMUL 1
#define VARLIST_SPISHIFTPERIOD 2
#define VARLIST_SPICLOCK 3
#define VARLIST_SPICMDBITS 4
#define VARLIST_SPICMDREADBITS 5
#define VARLIST_SPICMDWRITEBITS 6
#define VARLIST_SPIMAXCLOCK 7

unsigned int g_nSpiMulChipNum=-1;
int g_nSpiPort=1;
unsigned char g_bCurrentOutput=BV_ALLOUT;
unsigned int g_nSpiShiftPeriod=1;
char g_szMaxClock[16]="1000";
char g_szErrorString[256]="No error";
unsigned int g_nError=SPIERR_NO_ERROR;
//Some gs_check data here. Maybe this indicates that this should be a file split ?
FT_HANDLE g_hFtDevice=0;

unsigned int g_nRef=0;
int g_nCmdReadBits=0;
int g_nCmdWriteBits=0;
int g_nSpiMul=0;
int g_nSpiMulConfig=0;
unsigned short g_nErrorAddress=0;
spifns_debug_callback g_pDebugCallback=0;
//END DEFINITIONS

void __cdecl spifns_debugout(const char *szFormat, ...);
void __cdecl spifns_debugout_readwrite(unsigned short nAddress, char cOperation, unsigned short nLength, unsigned short *pnData);

//LPT2FTDI DONE
void __cdecl spifns_close_port() {
	//if (g_hDevice) {
	if (g_hFtDevice) {
		//CloseHandle(g_hDevice);	//LPT2FTDI
		//g_hDevice=0;
		FT_Close(g_hFtDevice); //DONE
		g_hFtDevice=0;
		g_nSpiPort=0;
	}
}
//LPT2FTDI DONE
HANDLE __cdecl spifns_open_port(int nPort) {
	/*OSVERSIONINFOA ovi;
	char szFilename[20];
	const unsigned char pTestBuffer[]={0xCB};	//Hiermit gehen auch CSB, MOSI, CLK auf HIGH

	ovi.dwOSVersionInfoSize=sizeof(ovi);
	GetVersionExA(&ovi);
	if (ovi.dwPlatformId!=VER_PLATFORM_WIN32_NT)
		return INVALID_HANDLE_VALUE;
	//LPT2FTDI start
	sprintf_s(szFilename,80,"\\\\.\\LPTSPI%d",nPort);
	HANDLE hDevice=CreateFileA(szFilename,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,0,0);
	if (hDevice==INVALID_HANDLE_VALUE)
		return INVALID_HANDLE_VALUE;
	DWORD nWritten;
	if (!WriteFile(hDevice,pTestBuffer,sizeof(pTestBuffer),&nWritten,0)) {
		CloseHandle(hDevice);
		return INVALID_HANDLE_VALUE;
	}
	return hDevice;
	//LPT2FTDI end
	*/
	FT_HANDLE ftHandle;
	FT_STATUS ftStatus;
	UCHAR ucMask = BV_ALLOUT;				//Set the above listed pins to OUTPUT
	unsigned char pTestBuf[]={BV_ALLOUT};	//Herewith CSB, MOSI, CLK and MUL go HIGH
	DWORD TestBytesWritten;

	ftStatus = FT_Open(nPort, &ftHandle);
	if (ftStatus != FT_OK)
	{
		if (g_bDebug)
			printf("> DBG: FTDI-Open failed.");
		return 0;
	}
	ftStatus = FT_SetBaudRate(ftHandle, FTDI_FREQ/16);
	if (ftStatus != FT_OK)
	{
		if (g_bDebug)
			printf("> DBG: FTDI-SetBaudRate failed.");
		FT_Close(ftHandle);
		return 0;
	}
	ftStatus = FT_SetBitMode(ftHandle, ucMask, FT_BITMODE_SYNC_BITBANG);
	if (ftStatus != FT_OK)
	{
		if (g_bDebug)
			printf("> DBG: FTDI-SetBitMode failed.");
		FT_Close(ftHandle);
		return 0;
	}
	ftStatus = FT_Write(ftHandle, pTestBuf, 1, &TestBytesWritten);
	g_bCurrentOutput = pTestBuf[0];
	FT_Read(ftHandle, pTestBuf, 1, &TestBytesWritten);
	if (ftStatus != FT_OK)
	{
		if (g_bDebug)
			printf("> DBG: FTDI-Write failed.");
		FT_Close(ftHandle);
		return 0;
	}
	return ftHandle;
}
//LPT2FTDI DONE RECHECK
bool __cdecl SPITransfer(int nInput, int nBits, int *pnRetval, bool bEndTransmission) {
	if (pnRetval)
		*pnRetval=0;
	//LPT2FTDI start
	/*if (!g_hDevice) {
		static const char szError[]="No LPT port selected";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nError=SPIERR_NO_LPT_PORT_SELECTED;
		return 0;
	}*/
	//LPT2FTDI end; new FTDI Code
	if (!g_hFtDevice) {
		static const char szError[]="No FTDI port selected";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nError=SPIERR_NO_LPT_PORT_SELECTED;
		return 0;
	}
	//LPT2FTDI DONE
	unsigned int nShiftPeriod=g_nSpiShiftPeriod;
	unsigned int nMaxDataLength=2*g_nSpiShiftPeriod*(nBits+4);
	unsigned int nDataWritten=0; //1;	//CHANGED, Nippey (Original 0)
	unsigned int bit;
	unsigned char *pDataInput=(unsigned char *)malloc(nMaxDataLength);
	unsigned char *pDataOutput=(unsigned char *)malloc(nMaxDataLength);
	if (!pDataInput || !pDataOutput) {
		free(pDataInput);
		free(pDataOutput);
		static const char szError[]="Malloc failed";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nError=SPIERR_MALLOC_FAILED;
		return 0;
	}

	//pDataInput[0] = g_bCurrentOutput; //CHANGED, Nippey

	//Repeat the last value <nShiftPeriod>-times
	if (nShiftPeriod) {
		memset(&pDataInput[nDataWritten],g_bCurrentOutput,nShiftPeriod);
		nDataWritten=nShiftPeriod;
	}

	//For every bit <bit> of the comitted data word with <nBits> bits....
	//MSB first!! Valid data on rising edge!
	unsigned char bOutput=g_bCurrentOutput;
	for (bit=nBits; bit; bit--) {
		//unsigned char *pDataInputIterator=&pDataInput[nDataWritten];
		//Set data bit
		if ((1<<(bit-1)) & nInput)
			bOutput|=BV_MOSI;
		else
			bOutput&=~BV_MOSI;

		bOutput&=~BV_CLK;		//Set clock low
		if (nShiftPeriod) {
			memset(&pDataInput[nDataWritten],bOutput,nShiftPeriod);
			nDataWritten+=nShiftPeriod;
			//pDataInputIterator+=nShiftPeriod;
		}
		bOutput|=BV_CLK;	//Set clock high
		if (nShiftPeriod) {
			memset(&pDataInput[nDataWritten],bOutput,nShiftPeriod);
			nDataWritten+=nShiftPeriod;
			//pDataInputIterator+=nShiftPeriod;
		}
	}

	g_bCurrentOutput=bOutput;
	if (bEndTransmission) {
		//nDataWritten-=nShiftPeriod;	//THIS DELETES LAST CLOCK EDGE!!! Trying without it, Nippey
		bOutput&=~BV_CLK;	//Set clock low
		if (nShiftPeriod) {
			memset(&pDataInput[nDataWritten],bOutput,nShiftPeriod);
			nDataWritten+=nShiftPeriod;
		}
		bOutput|=BV_CSB;
		g_bCurrentOutput=bOutput;
		if (nShiftPeriod) {
			memset(&pDataInput[nDataWritten],bOutput,nShiftPeriod);
			nDataWritten+=nShiftPeriod;
			
			memset(&pDataInput[nDataWritten],bOutput|BV_CLK,nShiftPeriod);
			nDataWritten+=nShiftPeriod;
			
			memset(&pDataInput[nDataWritten],bOutput,nShiftPeriod);
			nDataWritten+=nShiftPeriod;
			
			memset(&pDataInput[nDataWritten],bOutput|BV_CLK,nShiftPeriod);
			nDataWritten+=nShiftPeriod;
		}
	}
	//loc10001496
	DWORD nBytesWritten, nBytesReturned;
	FT_STATUS writeStatus, readStatus;
	//LPT2FTDI start
	/*if (!DeviceIoControl(g_hDevice,0,pDataInput,nDataWritten,pDataOutput,nDataWritten,&nBytesReturned,0))
	{
		static const char szError[]="IOCTL failed";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nError=SPIERR_IOCTL_FAILED;
		free(pDataInput);
		free(pDataOutput);
		return 0;
	}*/
	//new FTDI Code
	if (g_bDebug)
		printf("> DBG: Writing/Reading %i Bytes.\n", nDataWritten);
	writeStatus = FT_Write(g_hFtDevice, pDataInput, nDataWritten, &nBytesWritten);
	readStatus = FT_Read(g_hFtDevice, pDataOutput, nDataWritten, &nBytesReturned);

	static bool printed = false;
	if (g_bDebug && !printed /*&& pnRetval!=0*/)	//This is the first read operation
	{
		_tprintf(TEXT("> DBG: Raw Data Stream (CLK,MOSI,MISO,CSB)\n"));
		for(unsigned int i=0; i<nDataWritten; i++)
			_tprintf(TEXT(" %s%s%s%s"),
				(pDataOutput[i]&BV_CLK)?TEXT("1"):TEXT("0"),
				(pDataOutput[i]&BV_MOSI)?TEXT("1"):TEXT("0"),
				(pDataOutput[i]&BV_MISO)?TEXT("1"):TEXT("0"),
				(pDataOutput[i]&BV_CSB)?TEXT("1"):TEXT("0"));
		//printed = true;
		_tprintf(TEXT("\n"));
	}

	if (writeStatus != FT_OK || readStatus != FT_OK)
	{
		static const char szError[]="FT Write/Read failed. (Error codes: %i/%i)";
		sprintf_s(g_szErrorString, sizeof(g_szErrorString)/sizeof(char),szError, writeStatus, readStatus);
		//memcpy(g_szErrorString,szError,sizeof(szError));
		g_nError=SPIERR_IOCTL_FAILED;
		free(pDataInput);
		free(pDataOutput);
		return 0;
	}

	//LPT2FTDI end
	unsigned int nDataRead=0;
	int nRetval=0;
	for (bit=nBits+1; bit; bit--) {
		nRetval*=2;
		if (pDataOutput[nDataRead] & BV_MISO)
			nRetval|=1;
		nDataRead+=2*g_nSpiShiftPeriod;
	}
	free(pDataInput);
	free(pDataOutput);
	if (pnRetval)
		*pnRetval=nRetval;
	return 1;
}
//PreTransmit:
// sets CSB high for two clocks: RESET INTERFACE
// sets CSB low for subsequent communication
bool __cdecl spifns_pre_transmit() {
	if (g_nSpiMul)
		g_bCurrentOutput|=BV_CSB;
	else
		g_bCurrentOutput=(g_bCurrentOutput & ~BV_MUL)|BV_CSB;
	bool retval=SPITransfer(0,2,0,0);
	if (g_nSpiMul)
		g_bCurrentOutput&=~BV_CSB;
	else {
		if (g_nSpiMulConfig)
			g_bCurrentOutput|=BV_CSB|BV_MUL;
		else
			g_bCurrentOutput&=~(BV_CSB|BV_MUL);
	}
	return retval;
}
bool __cdecl spifns_sequence_read_start(unsigned short nAddress) {
	unsigned char bReadCommand=(g_nCmdReadBits&~3)|3; //Filtering out the top two bits here is not strictly needed, but is nicer code-wise.
	if (g_bDebug) printf("> DBG: SendReadCmd\n");
	if (!SPITransfer(bReadCommand,8,0,0))
		return false;
	int nReturn;
	if (g_bDebug) printf("> DBG: SendReadAddress\n");
	if (!SPITransfer(nAddress,16,0,0) || !SPITransfer(0,16,&nReturn,0))
		return false;
	//Cast to short to drop any additional bits
	if (((unsigned short)nReturn) != ((nAddress >> 8)|(bReadCommand<<8))) {
		spifns_debugout_readwrite(nAddress,'r',0,0);
		const char szError[]="Unable to read";
		memcpy(g_szErrorString,szError,sizeof(szError));
		g_nErrorAddress=nAddress;
		g_nError=SPIERR_READ_FAILED;
		return false;
	}
	return true;
}
int __cdecl spifns_sequence_read(unsigned short nAddress, unsigned short nLength, unsigned short *pnOutput) {
	if (g_bDebug) printf("> DBG: Read:PreTransmit\n");
	if (!spifns_pre_transmit())
		return 1;
	//loc_10001D10
	if (g_bDebug) printf("> DBG: Read:Address\n");
	if (!spifns_sequence_read_start(nAddress))
		return 1;
	//Not accurate, but output should be the same.
	if (g_bDebug) printf("> DBG: Read:Data\n");
	unsigned short *pnOutputIterator=pnOutput;
	for (unsigned short i=nLength; i>0; i--) {
		int nTemp;
		if (!SPITransfer(0,16,&nTemp,i==1))
			return 1;
		*pnOutputIterator=(unsigned short)nTemp;
		pnOutputIterator++;
	}
	//loc_10001D63
	spifns_debugout_readwrite(nAddress,'r',nLength,pnOutput);
	return 0;
}
int __cdecl spifns_sequence_write(unsigned short nAddress, unsigned short nLength, unsigned short *pnInput) {
	spifns_debugout_readwrite(nAddress,'w',nLength,pnInput);
	if (g_bDebug) printf("> DBG: Write:PreTransmit\n");
	if (!spifns_pre_transmit())
		return 1;
	if (g_bDebug) printf("> DBG: Write:Cmd\n");
		if (!SPITransfer((g_nCmdWriteBits&~3)|2,8,0,0))
		return 1;
	if (g_bDebug) printf("> DBG: Write:Address\n");
	if (!SPITransfer(nAddress,16,0,0))
		return 1;
	if (g_bDebug) printf("> DBG: Write:Data\n");
	while (nLength) {
		if (!SPITransfer(*(pnInput++),16,0,0))
			return 1;
		nLength--;
	}
	return 0;
}
void __cdecl spifns_sequence_setvar_spishiftperiod(int nPeriod) {
	spifns_debugout("Delays set to %d\n",g_nSpiShiftPeriod=nPeriod);
}
//LPT2FTDI DONE
bool __cdecl spifns_sequence_setvar_spiport(int nPort) {
	spifns_close_port();
	//if (INVALID_HANDLE_VALUE==(g_hDevice=spifns_open_port(nPort)))	//LPT2FTDI
	if (g_bDebug)
		printf("> DBG: Open FTDI-Port %i...\n", nPort);
	if (0==(g_hFtDevice=spifns_open_port(nPort)))	//LPT2FTDI DONE
		return false;
	if (g_bDebug)
		printf("> DBG: Success.\n");
	g_nSpiPort=nPort;
	g_nSpiShiftPeriod=1;
	return true;
}
void __cdecl spifns_sequence_setvar_spimul(unsigned int nMul) {
	BYTE bNewOutput=g_bCurrentOutput&~BV_CLK;
	if ((g_nSpiMulChipNum=nMul)<=16) {
		//Left side
		if (g_nSpiMulConfig != nMul || g_nSpiMul != 1)
			g_nSpiShiftPeriod=1;
		g_nSpiMulConfig=nMul;
		g_nSpiMul=1;
		bNewOutput=(bNewOutput&(BV_CLK|BV_MOSI|BV_CSB))|((BYTE)nMul<<1);
	} else {
		//Right side
		//loc_10001995
		if (g_nSpiMulConfig != 0 || g_nSpiMul!=0)
			g_nSpiShiftPeriod=1;
		g_nSpiMul=0;
		g_nSpiMulConfig=0;
		bNewOutput&=BV_CLK|BV_MOSI|BV_CSB;
	}
	g_bCurrentOutput=bNewOutput;
	SPITransfer(0,2,0,0);
	LARGE_INTEGER liFrequency,liPCStart,liPCEnd;
	QueryPerformanceFrequency(&liFrequency);
	QueryPerformanceCounter(&liPCStart);
	do {
		Sleep(0);
		QueryPerformanceCounter(&liPCEnd);
	} while (1000 * (liPCEnd.QuadPart - liPCStart.QuadPart) / liFrequency.QuadPart < 5);
	spifns_debugout("MulitplexSelect: %04x mul:%d config:%d chip_num:%d\n",g_bCurrentOutput,g_nSpiMul,g_nSpiMulConfig,g_nSpiMulChipNum);
}

int __cdecl spifns_sequence_setvar(const char *szName, const char *szValue) {
	if (szName==0)
		return 1;
	if (szValue==0)
		return 1;
	long nValue=strtol(szValue,0,0);
	for (unsigned int i=0; i<(sizeof(g_pVarList)/sizeof(*g_pVarList)); i++) {
		if (_stricmp(szName,g_pVarList[i].szName)==0) {
			switch (i) {
			case VARLIST_SPISPORT:{
				if (!spifns_sequence_setvar_spiport(nValue)) {
					const char szError[]="Couldn't find FTDI port";	//LPT2FTDI
					memcpy(g_szErrorString,szError,sizeof(szError));
					return 1;
				}
								  }break;
			case VARLIST_SPIMUL:{
				spifns_sequence_setvar_spimul(nValue);
								}break;
			case VARLIST_SPISHIFTPERIOD:{
				spifns_sequence_setvar_spishiftperiod(nValue);
										}break;
			case VARLIST_SPICLOCK:{
				double dblValue=strtod(szValue,0);
				static const double dblNull=0.0;
				if (dblValue==dblNull)
					return 1; //ERROR!
				int nShiftPeriod=(int)((172.41/dblValue)+0.5);
				if (nShiftPeriod<1) nShiftPeriod=1;
				spifns_sequence_setvar_spishiftperiod(nShiftPeriod);
								  }break;
			case VARLIST_SPICMDBITS:
			case VARLIST_SPICMDREADBITS:
			case VARLIST_SPICMDWRITEBITS:{
				if (i!=VARLIST_SPICMDREADBITS)
					g_nCmdWriteBits=nValue;
				if (i!=VARLIST_SPICMDWRITEBITS)
					g_nCmdReadBits=nValue;
										 }break;
			case VARLIST_SPIMAXCLOCK:{
				double fValue=atof(szValue);
				if (fValue<1.0)
					fValue=1.0;
				_snprintf_s(g_szMaxClock,16,10,"%f",fValue); //I have no idea why they only allow 11 characters to be output everything indicates the stack could allocate 16 :/
									 }break;
			}
		}
	}
	return 0;
}


//EXPORTED

DLL_EXPORT int __cdecl spifns_init() {
	g_nRef+=1;
	return 0;
}
DLL_EXPORT void __cdecl spifns_close() {
	g_nRef--;
	if (g_nRef==0)
		spifns_close_port();
}

DLL_EXPORT void __cdecl spifns_getvarlist(const SPIVARDEF **ppList, unsigned int *pnCount) {
	*ppList=g_pVarList;
	*pnCount=sizeof(g_pVarList)/sizeof(*g_pVarList);
}
DLL_EXPORT const char * __cdecl spifns_getvar(const char *szName) {
	if (!szName) {
		return "";
	} else if (_stricmp(szName,"SPIPORT")==0) {
		static char szReturn[20];
		sprintf_s(szReturn,20,"%d",g_nSpiPort);
		return szReturn;
	} else if (_stricmp(szName,"SPIMUL")==0) {
		static char szReturn[20];
		sprintf_s(szReturn,20,"%d",g_nSpiMulChipNum);
		return szReturn;
	} else if (_stricmp(szName,"SPISHIFTPERIOD")==0) {
		static char szReturn[20];
		sprintf_s(szReturn,20,"%d",g_nSpiShiftPeriod);
		return szReturn;
	} else if (_stricmp(szName,"SPICLOCK")==0) {
		static char szReturn[64];
		sprintf_s(szReturn,64,"%f",172.41/(double)g_nSpiShiftPeriod);
		return szReturn;
	} else if (_stricmp(szName,"SPIMAXCLOCK")==0) {
		static char szReturn[24];
		sprintf_s(szReturn,24,"%s",g_szMaxClock);
		return szReturn;
	} else {
		return "";
	}
}
DLL_EXPORT int __cdecl spifns_get_version() {
	return SPIFNS_VERSION;
}
//LPT2FTDI DONE
DLL_EXPORT void __cdecl spifns_enumerate_ports(spifns_enumerate_ports_callback pCallback, void *pData) {
	char szPortName[10];
	/*
	sprintf_s(szPortName,8,"LPT1");
	pCallback(g_nSpiPort,szPortName,pData);
	for (int nPort=2; nPort<=16; nPort++) {
		//LPT2FTDI start
		HANDLE hDevice;
		if ((hDevice=spifns_open_port(nPort))!=INVALID_HANDLE_VALUE) {
			CloseHandle(hDevice);
			sprintf_s(szPortName,8,"LPT%d",nPort);
			pCallback(nPort,szPortName,pData);
		}
		//LPT2FTDI end
	}
	return;
	*/
	//NEW FTDI Part start
	FT_STATUS ftStatus;
	
	if (g_bDebug)
		printf("> Number of FTDI-ports: %i\n", g_NumDevs);
	for(unsigned int i=0; i<g_NumDevs; i++)
	{
		FT_HANDLE ftHandle;
		LONG portNumber;
			
		ftStatus = FT_Open(i,&ftHandle);
		if (ftStatus == FT_OK)
		{
			ftStatus = FT_GetComPortNumber(ftHandle,&portNumber);
			if (ftStatus == FT_OK)
			{
				if (g_bDebug)
					printf(" > Desc: %s, Flags: 0x%x, Handle: 0x%x, ID: 0x%x, \n   LocID: %i, SN: %s, Type: 0x%x, Port: %i\n", g_DevInfo[i].Description, g_DevInfo[i].Flags, g_DevInfo[i].ftHandle, g_DevInfo[i].ID, g_DevInfo[i].LocId, g_DevInfo[i].SerialNumber, g_DevInfo[i].Type, portNumber);

				sprintf_s(szPortName,10,"FTDI%d",i);
				pCallback(i,szPortName,pData);
			}
			ftStatus = FT_Close(ftHandle);
		}
	}
	//NEW FTDI Part end
}
DLL_EXPORT void __cdecl spifns_chip_select(int nChip) {
	g_bCurrentOutput=g_bCurrentOutput&(BV_MOSI|BV_CLK|BV_CSB);
	g_nSpiMul=0;
	g_nSpiMulConfig=nChip;
	g_nSpiMulChipNum=nChip;
	spifns_debugout(
		"ChipSelect: %04x mul:%d confog:%d chip_num:%d\n",
		g_bCurrentOutput,
		0,
		nChip,
		nChip);
}
DLL_EXPORT const char* __cdecl spifns_command(const char *szCmd) {
	if (_stricmp(szCmd,"SPISLOWER")==0) {
		if (g_nSpiShiftPeriod<40) {
			//SPI Shift Period seems to be done about 1.5 times, plus 1 to compensate for rounding down (for example in 1)
			g_nSpiShiftPeriod=g_nSpiShiftPeriod + (g_nSpiShiftPeriod>>2) + 1;  //>>2 => /2 ?
			spifns_debugout("Delays now %d\n",g_nSpiShiftPeriod);
		}
	}
	return 0;
}
DLL_EXPORT unsigned int __cdecl spifns_get_last_error(unsigned short *pnErrorAddress, const char **pszErrorString) {
	if (pnErrorAddress)
		*pnErrorAddress=g_nErrorAddress;
	if (pszErrorString)
		*pszErrorString=g_szErrorString;
	return g_nError;
}
DLL_EXPORT int __cdecl spifns_bluecore_xap_stopped() {
	if (!spifns_pre_transmit())
		return -1;
	if (!SPITransfer(0,0,0,0))
		return -1;
	int nSpiRetval;
	if (!SPITransfer(0,0,&nSpiRetval,0))
		return -1;
	if (!spifns_pre_transmit())
		return -1;
	g_bCurrentOutput&=~BV_CLK; //Set Chip Select low
	if (!nSpiRetval)
		return 0; //Not stopped
	unsigned short nTemp;
	if (spifns_sequence_read(0xFF9A,0x0001,&nTemp) == 0) {
		return 1;
	}
	g_nSpiShiftPeriod=1;
	return -1;
}
DLL_EXPORT int __cdecl spifns_sequence(SPISEQ *pSequence, unsigned int nCount) {
	int nRetval=0;
	while (nCount--) {
		switch (pSequence->nType) {
		case SPISEQ::TYPE_READ:{
			if (spifns_sequence_read(pSequence->rw.nAddress,pSequence->rw.nLength,pSequence->rw.pnData)==1)
				nRetval=1;
			if (!spifns_pre_transmit())
				nRetval=1;
							   }break;
		case SPISEQ::TYPE_WRITE:{
			if (spifns_sequence_write(pSequence->rw.nAddress,pSequence->rw.nLength,pSequence->rw.pnData)==1)
				nRetval=1;
			if (!spifns_pre_transmit())
				nRetval=1;
								}break;
		case SPISEQ::TYPE_SETVAR:{
			if (spifns_sequence_setvar(pSequence->setvar.szName,pSequence->setvar.szValue)==1)
				nRetval=1;
								 }break;

		}
		pSequence++;
	}
	return nRetval;
}

DLL_EXPORT void __cdecl spifns_set_debug_callback(spifns_debug_callback pCallback) {
	g_pDebugCallback=pCallback;
}

//EXPORT ENDE

void __cdecl spifns_debugout(const char *szFormat, ...) {
	if (g_pDebugCallback) {
		static char szDebugOutput[256];
		va_list args;
		va_start(args,szFormat);
		vsprintf_s(szDebugOutput,256,szFormat,args);
		g_pDebugCallback(szDebugOutput);
		va_end(args);
	}
}
void __cdecl spifns_debugout_readwrite(unsigned short nAddress, char cOperation, unsigned short nLength, unsigned short *pnData) {
	if (g_pDebugCallback) {
		static const char * const pszTable[]={
			"%04X     %c ????\n",
			"%04X     %c %04X\n",
			"%04X-%04X%c %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X %04X %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X %04X %04X %04X %04X ...\n"
		};
		unsigned short bCopy[8];
		if (pnData)
			memcpy(bCopy,pnData,sizeof(unsigned short)*((nLength<8)?nLength:8));
		else
			memset(bCopy,0,sizeof(bCopy));
		if (nLength<2) {
			spifns_debugout(pszTable[nLength],nAddress,cOperation,bCopy[0]);
		} else {
			spifns_debugout(pszTable[((nLength<9)?nLength:9)],nAddress,nAddress+nLength-1,cOperation,bCopy[0],bCopy[1],bCopy[2],bCopy[3],bCopy[4],bCopy[5],bCopy[6],bCopy[7]);
		}
	}
}
