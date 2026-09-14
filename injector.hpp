#pragma once
#include <Windows.h>
#include <string>
#include <Psapi.h>
#include <iostream>
#include <TlHelp32.h>
#include <filesystem>


class loadlibary_injector
{
private:
	// private variables to store stuff we will need for the injection
	DWORD process_id = 0;
	HANDLE process_handle = nullptr;
	std::string process_name = "";
	std::string dll_path = "";

	// nice print for the debugging makes it easier to see what the error is
	void print_info(const std::string& message, bool is_error = false)
	{
		// only print the message if we are in debug mode
#ifdef _DEBUG
		std::cout << (is_error ? "[-] " : "[+] ") << message << std::endl;
#endif
	}

	// function to get the process ID
	bool get_process_id()
	{
		// check if the process name is empty
		if (process_name.empty())
		{
			print_info("Process name is empty.", true);
			return false;
		}

		// create snapshot of all processes currently running 
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snapshot == INVALID_HANDLE_VALUE)
		{
			print_info("Failed to create process snapshot.", true);
			return false;
		}

		PROCESSENTRY32W process_entry{};
		process_entry.dwSize = sizeof(PROCESSENTRY32W);

		// convert the process name to a wide string for comparison
		std::wstring process_name_w(process_name.begin(), process_name.end());

		// loop through all processes in the snapshot to find the one we want
		bool found = false;
		if (Process32First(snapshot, &process_entry))
		{
			do
			{
				// using _wcsicmp to compare wide string process names case insensitively
				if (_wcsicmp(process_name_w.c_str(), process_entry.szExeFile) == 0)
				{
					process_id = process_entry.th32ProcessID;
					found = true;
					break;
				}
			} while (Process32Next(snapshot, &process_entry));
		}

		// close the snapshot handle after we are done with it
		CloseHandle(snapshot);

		if (!found)
		{
			print_info("Process not found.", true);
			return false;
		}

		return true;
	}
	
	// function to get the process handle
	bool get_handle()
	{
		// check if the process ID is valid
		if (process_id == 0)
		{
			print_info("Process ID is 0. Cannot get handle.", true);
			return false;
		}

		// open handle to process with the permissions we need for injection
		process_handle = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, process_id);

		// check if the handle is valid
		if (process_handle == nullptr || process_handle == INVALID_HANDLE_VALUE)
		{
			print_info("Failed to open process.", true);
			return false;
		}
		return true;
	}

	// function to inject the dll
	bool inject()
	{
		// check the handle is valid
		if (process_handle == nullptr || process_handle == INVALID_HANDLE_VALUE)
		{
			print_info("Process handle is invalid. Cannot inject.", true);
			return false;
		}
		
		// check the dll path is valid 
		if (dll_path.empty())
		{
			print_info("DLL path is empty. Cannot inject.", true);
			return false;
		}
		
		// get the absolute path of dll and make sure it actually exists
		std::error_code ec;
		std::string absolute_dll_path = std::filesystem::absolute(dll_path, ec).string();

		if (ec || !std::filesystem::exists(absolute_dll_path))
		{
			print_info("Failed to get absolute DLL path: " + ec.message(), true);
			return false;
		}

		// +1 for the null terminator because LoadLibraryA expects it to be null terminated
		size_t pathSize = absolute_dll_path.length() + 1;

		// allocate memory in the target process 
		LPVOID remote_buffer = VirtualAllocEx(process_handle, nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

		// check if the memory allocation was successful
		if (remote_buffer == nullptr)
		{
			print_info("Failed to allocate memory in target process.", true);
			return false;
		}

		// write dll path to previously allocated memory
		if (!WriteProcessMemory(process_handle, remote_buffer, absolute_dll_path.c_str(), pathSize, nullptr))
		{
			print_info("Failed to write DLL path to target process memory.", true);
			VirtualFreeEx(process_handle, remote_buffer, 0, MEM_RELEASE);
			return false;
		}

		// get the address of LoadLibraryA from kernel32.dll
		HMODULE kernel32_module = GetModuleHandleA("kernel32.dll");
		FARPROC load_library_address = GetProcAddress(kernel32_module, "LoadLibraryA");

		// check if we got the address successfully
		if (load_library_address == nullptr)
		{
			print_info("Failed to get address of LoadLibraryA.", true);
			VirtualFreeEx(process_handle, remote_buffer, 0, MEM_RELEASE);
			return false;
		}

		// create remote thread in process to call LoadLibraryA with
		HANDLE remote_thread = CreateRemoteThread(process_handle, nullptr, 0, (LPTHREAD_START_ROUTINE)load_library_address, remote_buffer, 0, nullptr);

		if (remote_thread == nullptr)
		{
			print_info("Failed to create remote thread in target process.", true);
			VirtualFreeEx(process_handle, remote_buffer, 0, MEM_RELEASE);
			return false;
		}

		// wait for the remote thread to finish 
		WaitForSingleObject(remote_thread, INFINITE);

		// clean up after ourself
		VirtualFreeEx(process_handle, remote_buffer, 0, MEM_RELEASE);
		CloseHandle(remote_thread);
		CloseHandle(process_handle);
		
		// set this so destructor doesnt try close it again since we closed it here
		process_handle = nullptr;

		return true;
	}

public:

	// constructor to initialize the injector with needed variables
	loadlibary_injector(const std::string& process_name, const std::string& dll_path)
		: process_name(process_name), dll_path(dll_path)
	{
		
	}

	// destructor to clean up making sure the process handle is closed if it was still open
	~loadlibary_injector()
	{
		if (process_handle != nullptr && process_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(process_handle);
		}
	}

	// main run function to call private functions in order to inject the dll into the process
	bool run()
	{
		// get process id if fails then prints error and returns
		if (!get_process_id())
		{
			print_info("Failed to get process ID for: " + process_name, true);
			return false;
		}
		// get process handle if fails then prints error and returns
		if (!get_handle())
		{
			print_info("Failed to get process handle for: " + process_name, true);
			return false;
		}
		// inject the dll into the process if it fails then prints error and returns
		if (!inject())
		{
			print_info("Failed to inject DLL into process: " + process_name, true);
			return false;
		}

		return true;
	}

};