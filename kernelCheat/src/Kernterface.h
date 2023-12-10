#pragma once

#include <Windows.h>

#define IO_READ_REQUEST CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0701 /* Our Custom Code */, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IO_WRITE_REQUEST CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0702 /* Our Custom Code */, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IO_GET_ID_REQUEST CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0703 /* Our Custom Code */, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IO_GET_MODULE_REQUEST CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0704 /* Our Custom Code */, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

typedef struct _KERNEL_READ_REQUEST
{
    ULONG ProcessId;

    ULONG Address;
    ULONG Response;
    ULONG Size;

} KERNEL_READ_REQUEST, * PKERNEL_READ_REQUEST;

typedef struct _KERNEL_WRITE_REQUEST
{
    ULONG ProcessId;

    ULONG Address;
    ULONG Value;
    ULONG Size;

} KERNEL_WRITE_REQUEST, * PKERNEL_WRITE_REQUEST;

class Kernterface {
public:
    HANDLE hDriver;

    Kernterface(LPCSTR RegPath)
    {
        hDriver = CreateFileA(RegPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    }

    template <typename T>
    T Read(ULONG ProcessId, ULONG ReadAddress, SIZE_T Size)
    {
        if (hDriver == INVALID_HANDLE_VALUE)
            return T(); // Return a default-constructed T when the driver handle is invalid

        DWORD Return, Bytes;
        KERNEL_READ_REQUEST ReadRequest;

        ReadRequest.ProcessId = ProcessId;
        ReadRequest.Address = ReadAddress;
        ReadRequest.Size = Size;

        if (DeviceIoControl(hDriver, IO_READ_REQUEST, &ReadRequest, sizeof(ReadRequest), &ReadRequest, sizeof(ReadRequest), 0, 0))
            return *reinterpret_cast<T*>(&ReadRequest.Response); // Cast the response to the requested type
        else
            return T(); // Return a default-constructed T in case of failure
    }

    bool Write(ULONG ProcessId, ULONG WriteAddress, auto WriteValue, SIZE_T WriteSize)
    {
        if (hDriver == INVALID_HANDLE_VALUE)
            return false;
        DWORD Bytes;

        KERNEL_WRITE_REQUEST WriteRequest;
        WriteRequest.ProcessId = ProcessId;
        WriteRequest.Address = WriteAddress;
        WriteRequest.Value = WriteValue;
        WriteRequest.Size = WriteSize;

        if (DeviceIoControl(hDriver, IO_WRITE_REQUEST, &WriteRequest, sizeof(WriteRequest), 0, 0, &Bytes, NULL))
            return true;
        else
            return false;
    }

    DWORD GetTargetPid()
    {
        if (hDriver == INVALID_HANDLE_VALUE)
            return false;

        ULONG Id;
        DWORD Bytes;

        if (DeviceIoControl(hDriver, IO_GET_ID_REQUEST, &Id, sizeof(Id), &Id, sizeof(Id), &Bytes, NULL))
            return Id;
        else
            return 0;
    }

    DWORD GetClientModule()
    {
        if (hDriver == INVALID_HANDLE_VALUE)
            return false;

        ULONG Address;
        DWORD Bytes;

        if (DeviceIoControl(hDriver, IO_GET_MODULE_REQUEST, &Address, sizeof(Address), &Address, sizeof(Address), &Bytes, NULL))
            return Address;
        else
            return 0;
    }
};
