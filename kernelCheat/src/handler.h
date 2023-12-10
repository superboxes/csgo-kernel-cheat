#pragma once
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <TlHelp32.h>
#include <string_view>

class Handler
{
private:
	std::uintptr_t processId = 0;
	void* processHandle = nullptr;

public:

	// open process func
	Handler(std::string_view processName) noexcept
	{
		::PROCESSENTRY32 entry = { };
		entry.dwSize = sizeof(::PROCESSENTRY32);

		const HANDLE snapShot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		while (::Process32Next(snapShot, &entry))
		{
			if (!processName.compare(entry.szExeFile))
			{
				processId = entry.th32ProcessID;
				processHandle = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
				break;
			}
		}

		// Free handle
		if (snapShot)
			::CloseHandle(snapShot);
	}

	// descructor to free handle
	~Handler()
	{
		if (processHandle)
			::CloseHandle(processHandle);
	}

	std::uintptr_t GetModuleAddress(std::string_view moduleName) const noexcept
	{
		::MODULEENTRY32 entry = { };
		entry.dwSize = sizeof(::MODULEENTRY32);

		const HANDLE snapShot = ::CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);

		std::uintptr_t result = 0;

		while (::Module32Next(snapShot, &entry))
		{
			if (!moduleName.compare(entry.szModule))
			{
				result = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
				break;
			}
		}

		if (snapShot)
			::CloseHandle(snapShot);

		return result;
	}

	// read from process function
	template <typename T>
	const T Read(const std::uintptr_t address) const noexcept
	{
		T value = { };
		::ReadProcessMemory(processHandle, reinterpret_cast<const void*>(address), &value, sizeof(T), NULL);
		return value;
	}

	// write data to process function
	template <typename T>
	void Write(const std::uintptr_t address, const T& value) const noexcept
	{
		::WriteProcessMemory(processHandle, reinterpret_cast<void*>(address), &value, sizeof(T), NULL);
	}

	bool IsProcessOn() const noexcept
	{
		return processHandle != nullptr;
	}
};
