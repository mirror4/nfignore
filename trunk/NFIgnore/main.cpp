#include <windows.h>
#include <string>
#include <iostream>
#include <vector>
#include <iterator>
#include <fstream>
#include <boost/thread.hpp>
#include <boost/bind.hpp>

using namespace boost;
using namespace std;

string GetCWD()
{
	TCHAR cwd[MAX_PATH] = "";
	if(!::GetCurrentDirectory(sizeof(cwd) - 1, cwd))
		throw runtime_error("could not get current directory");

	return string(cwd);
}

//this returns a handle to the injected dll
HMODULE InjectDll(HANDLE process, string dll)
{

//make the dll name a full path
	dll = GetCWD()+'\\'+dll;

//write the dll name into the remote process' memory
	void* libstr = ::VirtualAllocEx(process, NULL, dll.size()+1, MEM_COMMIT, PAGE_READWRITE);
	if (!libstr)
		throw runtime_error("could not allocate memory in remote process");

	if (!::WriteProcessMemory(process, libstr, static_cast<const void*>(dll.c_str()), dll.size()+1, NULL))
		throw runtime_error("could not write to remote memory");


//call LoadLibrary in a thread of the remote process
	HANDLE thread = ::CreateRemoteThread(process, NULL, 0,
		(LPTHREAD_START_ROUTINE)::GetProcAddress(::GetModuleHandle("Kernel32"), "LoadLibraryA"),
		libstr, 0, NULL);
	::WaitForSingleObject(thread, INFINITE);

//get handle of loaded module
	DWORD libModule;
	::GetExitCodeThread(thread, &libModule);

	::CloseHandle(thread);
	::VirtualFreeEx(process, libstr, dll.size(), MEM_RELEASE);

	return reinterpret_cast<HMODULE>(libModule);
}

bool FreeDll(HANDLE process, HMODULE dll)
{
//call FreeLibrary in the remote process
	HANDLE thread = ::CreateRemoteThread(process, NULL, 0,
		reinterpret_cast<LPTHREAD_START_ROUTINE>(::GetProcAddress(::GetModuleHandle("Kernel32"), "FreeLibrary")),
		reinterpret_cast<void*>(dll), 0, NULL);

	::WaitForSingleObject(thread, INFINITE);

	DWORD exitStatus;
	::GetExitCodeThread(thread, &exitStatus);

	::CloseHandle(thread);

	return exitStatus;
}

BOOL SetPrivilege( 
	HANDLE hToken,  // token handle 
	LPCTSTR Privilege,  // Privilege to enable/disable 
	BOOL bEnablePrivilege  // TRUE to enable. FALSE to disable 
) 
{ 
	TOKEN_PRIVILEGES tp = { 0 }; 
	// Initialize everything to zero 
	LUID luid; 
	DWORD cb=sizeof(TOKEN_PRIVILEGES); 
	if(!LookupPrivilegeValue( NULL, Privilege, &luid ))
		return FALSE; 
	tp.PrivilegeCount = 1; 
	tp.Privileges[0].Luid = luid; 
	if(bEnablePrivilege) { 
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
	} else { 
		tp.Privileges[0].Attributes = 0; 
	} 
	AdjustTokenPrivileges( hToken, FALSE, &tp, cb, NULL, NULL ); 
	if (GetLastError() != ERROR_SUCCESS) 
		return FALSE; 

	return TRUE;
}


void EnablePrivileges()
{
	HANDLE hToken;
	if(!OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &hToken))
    {
        if (GetLastError() == ERROR_NO_TOKEN)
        {
            if (!ImpersonateSelf(SecurityImpersonation))
				throw runtime_error("could not get token");

            if(!OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &hToken)){
				throw runtime_error("could not get token");
            }
         }
        else
            throw runtime_error("could not get token");
     }
	if (!SetPrivilege(hToken, SE_DEBUG_NAME, TRUE))
		throw runtime_error("could not enable privileges");
	CloseHandle(hToken);
}


HANDLE GetProcessHandle(string windowName)
{
	HWND wh = ::FindWindow(windowName.c_str(), windowName.c_str());
	DWORD pid;
	::GetWindowThreadProcessId(wh, &pid);
	
	EnablePrivileges();
	return OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

}

void clear()
{
	system("cls");
}


void printFile(string file, string prefix)
{
	cout << prefix << endl;

	ifstream users_file(file.c_str());

	copy(istream_iterator<string>(users_file), istream_iterator<string>(), ostream_iterator<string>(cout, "\n"));
	cout << flush;
}


HANDLE nf;
HMODULE dHandle,hHandle;

void onExit()
{

	FreeDll(nf, hHandle);
	FreeDll(nf, dHandle);
}

typedef void(*reload_t)(string);

//when the users file changes
void reload(string file)
{
	
	FreeDll(nf, hHandle);
	hHandle = InjectDll(nf, "hook.dll");
	clear();

	printFile("ignore_users.txt", "Ignoring:");
	printFile("ignore_phrases.txt", "---");

	dHandle = InjectDll(nf, "detoured.dll");
	hHandle = InjectDll(nf, "hook.dll");
}

template<class F>
bool WatchFiles(string dir, F callback)
{


	HANDLE h = FindFirstChangeNotification (dir.c_str(), FALSE,	FILE_NOTIFY_CHANGE_LAST_WRITE);
	if (h == INVALID_HANDLE_VALUE)
	{
		cout << "could not obtain handle for " << dir << endl;
	}

	while (true)
	{
		DWORD r = WaitForSingleObject(h, INFINITE);

		if (WAIT_ABANDONED_0 == r)
		{
			cout << "wait abandoned" << endl;
			//cout << files[r-WAIT_ABANDONED_0] << endl;
			return true;
		}
		if (WAIT_FAILED == r)
		{
			DWORD e = GetLastError();
			cout << "wait failed: " << e << endl;
			return false;
		}
		
	//get what file changed
		
		HANDLE dirh = CreateFile(dir.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (dirh == INVALID_HANDLE_VALUE)
		{
			cout << "could not get handle to dir" << GetLastError() << endl;
			return false;
		}

		DWORD filelen=0;
		TCHAR file[MAX_PATH] = "";
		if (!ReadDirectoryChangesW(dirh, file, MAX_PATH-1, false, FILE_NOTIFY_CHANGE_LAST_WRITE, &filelen, NULL, NULL))
		{
			cout << "could not read dir changes " << GetLastError() << endl;
			return false;
		}

		FILE_NOTIFY_INFORMATION* ff = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(file);
		callback(string(ff->FileName, ff->FileName+(ff->FileNameLength/2)));

		// data.dwFilesizeLow has the size information

		FindNextChangeNotification (h);
	};

	return true;
}

int main(int agrc, char** argv)
{
	clear();

//get the handle for navyfield with full access rights
	nf = GetProcessHandle("NavyFIELD");
	if (!nf)
	{
		cout << "Waiting for NavyFIELD to start..." << endl;
		while (!nf) { nf =  GetProcessHandle("NavyFIELD"); }
	}
	
	dHandle = InjectDll(nf, "detoured.dll");
	hHandle = InjectDll(nf, "hook.dll");
	atexit(onExit);


	printFile("ignore_users.txt", "Ignoring:");
	printFile("ignore_phrases.txt", "---");


//this gives notifications when our files change.
//  we can then reload the ignore lists
	thread t (bind(&WatchFiles<reload_t>, GetCWD(), &reload));

	while(true)
	{
		char in;
		cin >> in;

		switch(in)
		{
		case 'q':
			return 0;
//not needed anymore (onExit, and watching for file changes)
/*		case 'r':
			//reload();
			break;
		case 'd':
			FreeDll(nf, hHandle);*/
		}
	}

	
	return 0;
}
