// *************************************************************
//  
//  Licensed to the Apache Software Foundation (ASF) under one
//  or more contributor license agreements.  See the NOTICE file
//  distributed with this work for additional information
//  regarding copyright ownership.  The ASF licenses this file
//  to you under the Apache License, Version 2.0 (the
//  "License"); you may not use this file except in compliance
//  with the License.  You may obtain a copy of the License at
//  
//    http://www.apache.org/licenses/LICENSE-2.0
//  
//  Unless required by applicable law or agreed to in writing,
//  software distributed under the License is distributed on an
//  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
//  KIND, either express or implied.  See the License for the
//  specific language governing permissions and limitations
//  under the License.
//  
// *************************************************************
#ifndef _PIPEIMPL_H_
#define _PIPEIMPL_H_

#ifndef _WINDOWS_
# ifdef _MSC_VER
#   pragma warning(push,1) /* disable warnings within system headers */
# endif
#	include <windows.h>
# ifdef _MSC_VER
#   pragma warning(pop)
# endif
#endif

#ifndef _INC_MALLOC
#	include <malloc.h>
#endif

#ifndef _INC_TCHAR
#	ifdef UNICODE
#		define _UNICODE
#	endif
#	include <tchar.h>
#endif

#define EXPORT_PIPE_API

//============================================================================
//	Helper functions
//============================================================================

// Because the value of an invalid HANDLE returned by API functions differs 
// between different functions and differs on different Windows platforms,
// this function checks wether the handle has a meaningful value.
#ifndef __cplusplus

#define IsValidHandle( handle ) ((DWORD)(handle) + 1 > 1)

#else

inline bool IsValidHandle( HANDLE handle )
{
	return INVALID_HANDLE_VALUE != handle && NULL != handle;
}

extern "C" {

#endif	// __cplusplus


EXPORT_PIPE_API HANDLE WINAPI CreateSimplePipeA( LPCSTR lpName );
EXPORT_PIPE_API HANDLE WINAPI CreateSimplePipeW( LPCWSTR lpName );

#ifdef UNICODE
#define CreateSimplePipe	CreateSimplePipeW
#else
#define CreateSimplePipe	CreateSimplePipeA
#endif

EXPORT_PIPE_API HANDLE WINAPI OpenSimplePipeA( LPCSTR lpName );
EXPORT_PIPE_API HANDLE WINAPI OpenSimplePipeW( LPCWSTR lpName );

#ifdef UNICODE
#define OpenSimplePipe	OpenSimplePipeW
#else
#define OpenSimplePipe	OpenSimplePipeA
#endif

EXPORT_PIPE_API HANDLE WINAPI AcceptSimplePipeConnection( HANDLE hPipe );

EXPORT_PIPE_API BOOL WINAPI WaitForSimplePipeA( LPCSTR lpName, DWORD dwTimeOut );
EXPORT_PIPE_API BOOL WINAPI WaitForSimplePipeW( LPCWSTR lpName, DWORD dwTimeOut );

#ifdef UNICODE
#define WaitForSimplePipe	WaitForSimplePipeW
#else
#define WaitForSimplePipe	WaitForSimplePipeA
#endif


EXPORT_PIPE_API BOOL WINAPI WriteSimplePipe( HANDLE hPipe, LPCVOID lpBuffer, DWORD dwBytesToWrite, LPDWORD lpBytesWritten, BOOL bWait );
EXPORT_PIPE_API BOOL WINAPI ReadSimplePipe( HANDLE hPipe, LPVOID lpBuffer, DWORD dwBytesToRead, LPDWORD lpBytesRead, BOOL bWait );
EXPORT_PIPE_API BOOL WINAPI CloseSimplePipe( HANDLE hPipe );

#ifdef __cplusplus
}	// extern "C"
#endif

#endif	// _PIPEIMPL_H_
