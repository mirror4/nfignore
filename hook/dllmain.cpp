#include <cstdio>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <string>
#include <iterator>
#include <vector>
#include <algorithm>
#include <boost/regex.hpp>

#include <winsock2.h>
#include <windows.h>
#include <detours.h>
#pragma comment( lib, "Ws2_32.lib" )
#pragma comment( lib, "detours.lib" )
#pragma comment( lib, "detoured.lib" )
#pragma comment( lib, "Mswsock.lib" )

using namespace std;
//ofstream test;

vector<string> users;
vector<string> phrases;

//this runs a regex match of text to all regexs in the containor
//it returns true is one or more match
template <class T>
bool matches(string text, T c)
{
	for (T::const_iterator i=c.begin(); i!=c.end(); ++i)
	{
		boost::regex e(*i);
		if (boost::regex_match(text, e))
		{
			return true;
		}
	}
	return false;
}

int ( WSAAPI *Real_WSARecv )(SOCKET s, 
	LPWSABUF lpBuffers,
    DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesRecvd,
    LPDWORD lpFlags,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
    ) = WSARecv;



int WSAAPI Mine_WSARecv(SOCKET s, 
	LPWSABUF lpBuffers,
    DWORD dwBufferCount,
    LPDWORD lpNumberOfBytesRecvd,
    LPDWORD lpFlags,
    LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
    )
{
	int ret = Real_WSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);

	if (lpOverlapped != NULL && lpCompletionRoutine != NULL)
	{
		//test << "***overlapped recv in use!" << endl;
		return ret;
	}

	if (ret != 0)
		return ret;

	char* buf = lpBuffers[0].buf;
	ULONG len = lpBuffers[0].len;

	//test for magic number for user messages
	if (len >= 0x45 &&
		buf[0] == 4 &&
		buf[1] == 3 &&
		buf[2] == 2 &&
		buf[3] == 1 &&
		buf[4] == 0 &&
		buf[5] == 10 && 
		buf[6] == 2 )
	{


		//i jsut pass in the pointer to the beginning of the string
		// (the constructor reads until \0 )
		string user(buf+0x14);
		string message(buf+0x44);
		
		if (user.size() && message.size())
			//test << user << ": " << message << endl;
			;
		else
			return 0;

		//perform another recieve if the user is in the ignored userlist
		// this in essance droppes the packet containing the message to be ignored
		if (binary_search(users.begin(), users.end(), user) ||
			matches(message, phrases))
		{
			return Mine_WSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
		}


	}

	return 0;
}

string GetDllPath(HINSTANCE hinst)
{
	TCHAR cwd[MAX_PATH] = "";
	::GetModuleFileName(hinst, cwd, sizeof(cwd) - 1);

	string ret(cwd);
	return ret.substr(0, ret.find_last_of('\\')+1);
}


template<class T>
void read(string path, T& container)
{
	fstream f(path.c_str());
	copy(istream_iterator<string>(f), istream_iterator<string>(), back_inserter(container));
	sort(container.begin(), container.end());
}



BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD dwReason, LPVOID ) 
{
	
	string cwd;

	switch ( dwReason ) 
	{
    case DLL_PROCESS_ATTACH:        
		//test.open("c:\\test.txt", ofstream::app);
		
		
		cwd = GetDllPath(hinstDLL);
		
		read(cwd+"ignore_users.txt", users);
		read(cwd+"ignore_phrases.txt", phrases);


        DetourTransactionBegin();
        DetourUpdateThread( GetCurrentThread() );
        DetourAttach( &(PVOID &)Real_WSARecv, Mine_WSARecv );

		switch (DetourTransactionCommit())
		{
		case NO_ERROR:
			//test << "detoured sucessfully" << endl;
			break;
		case ERROR_INVALID_DATA:
			//test << "invalid data" << endl;
			break;
		case ERROR_INVALID_OPERATION:
			//test << "invalid op" << endl;
			break;
		default:
			;
			//test << "unknown error detouring" << endl;
		}

        break;

    case DLL_PROCESS_DETACH:

        DetourTransactionBegin();
        DetourUpdateThread( GetCurrentThread() );

        DetourDetach( &(PVOID &)Real_WSARecv, Mine_WSARecv );
        DetourTransactionCommit();
		//test << "detac" << endl;
		//test.close();

        break;
    }

    return TRUE;
}
