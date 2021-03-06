/*********************************************************
* Copyright (C) VERTVER, 2018. All rights reserved.
* OpenSoundRefenation - WINAPI open-source DAW
* MIT-License
**********************************************************
* Module Name: OSR network
**********************************************************
* osrNetworkRecord.c
* Network system implementation
*********************************************************/
#include "osrNetwork.h"

SOCKET CurrentSocket = 0;
WSADATA wsData = { 0 };

BOOL
WINAPI
InitNetworkModule()
{
	// init WinSocket API
	BOOL bResult = WSAStartup(MAKEWORD(2, 2), &wsData);
	if (!!bResult) { return FALSE; }

	return TRUE;
}

BOOL
WINAPI
FreeNetworkModule()
{
	// close WinSocket API 
	BOOL bResult = WSACleanup();
	if (!!bResult) { return FALSE; }

	return TRUE;
}

BOOL
WINAPI
SetNetworkConnection(
	LPCSTR lpPort,
	LPSTR lpOutAddress,
	WORD* lpOutPort
)	
{
	ADDRINFOA* pResult = NULL;
	ADDRINFOA networkResult = { 0 };
	SOCKADDR_IN sockAddress = { 0 };
	SOCKET ListenSocket = INVALID_SOCKET;
	int SocketSize = sizeof(SOCKADDR_IN);
	char lpString[128] = { 0 };

	networkResult.ai_family = AF_INET;
	networkResult.ai_socktype = SOCK_DGRAM;
	networkResult.ai_protocol = IPPROTO_UDP;
	networkResult.ai_flags = AI_PASSIVE;

	// get current network information
	BOOL bResult = GetAddrInfoA(NULL, lpPort, &networkResult, &pResult);
	if (!!bResult) { return FALSE; }

	// create new network socket
	ListenSocket = socket(pResult->ai_family, pResult->ai_socktype, pResult->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) { return FALSE; }

	// bind socket to work with network
	bResult = bind(ListenSocket, pResult->ai_addr, (INT)pResult->ai_addrlen);
	if (bResult == SOCKET_ERROR) 
	{
		closesocket(ListenSocket);
		FreeAddrInfoA(pResult);
		return FALSE; 
	}

	// get socket information
	bResult = getsockname(ListenSocket, (SOCKADDR*)&sockAddress, &SocketSize);
	if (bResult == SOCKET_ERROR) 
	{
		closesocket(ListenSocket);
		FreeAddrInfoA(pResult);
		return FALSE;
	}

	if (!lpOutAddress) 
	{
		closesocket(ListenSocket);
		FreeAddrInfoA(pResult);
		return FALSE;
	}

	//#NOTE: inet_addr is deprecated in Win 6.0 or greater
	if (IsWindowsXPOrGreater())
	{
		InetNtopA(AF_INET, &sockAddress.sin_addr.s_addr, lpString, sizeof(lpString));
	}
	else
	{
#if _WIN32_WINNT <= 0x0501
		lpString = inet_ntoa(sockAddress.sin_addr)
#endif
	}

	strcpy_s(lpOutAddress, strlen(lpString), lpString);

	if (!lpOutPort)
	{
		closesocket(ListenSocket);
		FreeAddrInfoA(pResult);
		return FALSE;
	}
	// get port of network
	*lpOutPort = htons(sockAddress.sin_port);

	// try to listen socket
	bResult = listen(ListenSocket, SOMAXCONN);
	if (bResult == SOCKET_ERROR)
	{
		// if we here, our socket is can't be listened
		closesocket(ListenSocket);
		FreeAddrInfoA(pResult);
		return FALSE;
	}

	// save current socket
	CurrentSocket = ListenSocket;

	FreeAddrInfoA(pResult);
	return TRUE;
}

BOOL
WINAPI
CloseNetworkConnection()
{
	if (CurrentSocket)
	{
		// try to shutdown socket
		BOOL bResult = shutdown(CurrentSocket, SD_SEND);
		if (bResult == SOCKET_ERROR)
		{
			// if we can't - close it
			closesocket(CurrentSocket);
			CurrentSocket = 0;
		}
	}

	return TRUE;
}

BOOL
WINAPI
SendDataToAddress(
	BYTE* lpData,
	WORD wDataSize,
	LPCSTR lpIpAddress,
	WORD wPort
)
{
	SOCKADDR_IN ClientAddress = { 0 };
	ClientAddress.sin_family = AF_INET;
	ClientAddress.sin_port = htons(wPort);

	//#NOTE: inet_addr is deprecated in Win 6.0 or greater
	if (IsWindowsVistaOrGreater())
	{
		InetPtonA(AF_INET, lpIpAddress, &ClientAddress.sin_addr.s_addr);	
	}
	else
	{
#if _WIN32_WINNT <= 0x0501
		ClientAddress.sin_addr.s_addr = inet_addr(lpIpAddress);
#endif 
	}

	// copy data to our buffer
	BYTE* pBuf = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, wDataSize);
	memcpy(pBuf, lpData, wDataSize);

	BOOL bResult = sendto(CurrentSocket, (LPCSTR)pBuf, wDataSize, 0, (SOCKADDR*)&ClientAddress, sizeof(SOCKADDR_IN));
	if (bResult == SOCKET_ERROR) { return FALSE; }
	
	HeapFree(GetProcessHeap(), 0, pBuf);

	return TRUE;
}

BOOL
WINAPI
GetDataFromSender(
	BYTE* lpData,
	WORD* wDataSize,
	LPCSTR lpIpAddress,
	WORD wPort
)
{
	SOCKADDR_IN ClientAddress = { 0 };
	static size_t sockIn = sizeof(SOCKADDR_IN);

	// copy data to our buffer
	BYTE* pBuf = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, wDataSize);
	BOOL bResult = recvfrom(CurrentSocket, pBuf, wDataSize, 0, (SOCKADDR*)&ClientAddress, &sockIn);
	if (bResult == SOCKET_ERROR) { return FALSE; }

	memcpy(pBuf, lpData, wDataSize);
	HeapFree(GetProcessHeap(), 0, pBuf);

	return TRUE;
}
