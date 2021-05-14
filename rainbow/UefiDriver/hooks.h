#pragma once

namespace Hooks
{
    inline INT8 originalData[15];
    inline INT8 newData[15];
    typedef INTN(__stdcall* IopLoadDriver_t)(VOID* KeyHandle);
    inline IopLoadDriver_t function;
    inline VOID* kernelBase;

    void Hook();
    void Unhook();

    VOID* GetModuleBase(const CHAR16* moduleName);
    EFI_STATUS ChangeDiskDispatch();
    EFI_STATUS ChangeNetworkDispatch();
    EFI_STATUS ZeroSmbiosData();

    INTN HookedIopLoadDriver(VOID* KeyHandle);
}