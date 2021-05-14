#include "general.h"

/*
 * Exports for the VisualUefi and EDK2 driver libraries
 */
EXTERN_C const UINT8 _gDriverUnloadImageCount = 1;
EXTERN_C const UINT32 _gUefiDriverRevision = 0x200;
EXTERN_C const UINT32 _gDxeRevision = 0x200;
/*
 * Requires you to add extern "C" to vshacks.h in VisualUefi
 */
EXTERN_C CHAR8* gEfiCallerBaseName = "BaseLib2";

typedef void(__stdcall* BlpArchSwitchContext_t)(int target);
BlpArchSwitchContext_t BlpArchSwitchContext;

EFI_EXIT_BOOT_SERVICES originalExitBootServices;

#define ContextPrint(x, ...) \
    BlpArchSwitchContext(FirmwareContext); \
    Print(x, __VA_ARGS__); \
    BlpArchSwitchContext(ApplicationContext); \

__forceinline VOID* GetVirtual(VOID* physical)
{
    /*
     * Convert physical address to virtual using EFI
     * runtime services
     * Usable only when calling SetVirtualAddressMap
     */
    VOID* address = physical;
    gRT->ConvertPointer(0, &address);
    return address;
}

EFI_EVENT virtualEvent = NULL;
VOID EFIAPI NotifySetVirtualAddressMap(EFI_EVENT Event, VOID* Context)
{
    /*
     * Believe it or not, we are currently in the application
     * context
     * You could probably not even use BlpArchSwitchContext and you do all
     * the crap from here, but I already wrote what I wrote so fuck it...
     */
    Utils::CopyMemory(Hooks::originalData, Hooks::function, 15);

    /*
     * Simple absolute jump, nothing fancy
     */
    UINT8 jump[] = { 0x48, 0x31, 0xc0, 0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xe0 };
    *reinterpret_cast<UINT64*>(reinterpret_cast<UINT64>(jump) + 5) = reinterpret_cast<UINT64>(GetVirtual(Hooks::HookedIopLoadDriver));

    Utils::CopyMemory(Hooks::newData, jump, 15);
    Hooks::Hook();
}

EFI_STATUS HookedExitBootServices(EFI_HANDLE ImageHandle, UINTN MapKey)
{
    /*
     * Will be pointing somewhere in OslFwpKernelSetupPhase1
     */
    UINT64 returnAddress = reinterpret_cast<UINT64>(_ReturnAddress());

    /*
     * Might be called multiple times by winload in a loop failing few times
     */
    gBS->ExitBootServices = originalExitBootServices;

    /*
     * Clean the screen and set the appropriate colors
     */
    gST->ConOut->SetAttribute(gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK);
    gST->ConOut->ClearScreen(gST->ConOut);
    Print(EW(L"OslFwpKernelSetupPhase1   -> (phys) 0x%p\n"), returnAddress);

    /*
     * OslFwpKernelSetupPhase1 -> 00000001800060D8
     * OslExecuteTransition    -> 000000018001639C
     * BlpArchSwitchContext    -> 000000018002CA48
     */

    /*
     * Start of OslExecuteTransition
     */
    UINT64 loaderBlockScan = Utils::FindPattern(reinterpret_cast<VOID*>(returnAddress), SCAN_MAX, E("48 8B 3D ? ? ? ? 48 8B 8F ? ? ? ?"));
    if (!loaderBlockScan)
    {
        Print(EW(L"Failed to find OslExecuteTransition!\n"));
        INFINITE_LOOP();
    }
    Print(EW(L"OslExecuteTransition      -> (phys) 0x%p\n"), loaderBlockScan);

    /*
     * Resolve relative address (instruction end address + instruction offset)
     */
    UINT64 resolvedAddress = *reinterpret_cast<UINT64*>((loaderBlockScan + 7) + *reinterpret_cast<int*>(loaderBlockScan + 3));
    Print(EW(L"OslLoaderBlock            -> (virt) 0x%p\n"), resolvedAddress);

    /*
     * BlpArchSwitchContext
     */
    BlpArchSwitchContext = reinterpret_cast<BlpArchSwitchContext_t>(Utils::FindPattern(reinterpret_cast<VOID*>(returnAddress), SCAN_MAX, E("40 53 48 83 EC 20 48 8B 15")));
    if (!BlpArchSwitchContext)
    {
        Print(EW(L"Failed to find BlpArchSwitchContext!\n"));
        INFINITE_LOOP();
    }
    Print(EW(L"BlpArchSwitchContext      -> (phys) 0x%p\n"), BlpArchSwitchContext);

    /*
     * Switch context to (as defined by winload) application context
     * Within this context only the virtual addresses are valid
     * Real/physical addressing is not used
     * We can not use any EFI services unless we switch back!
     * To print on screen use ContextPrint define
     */
    BlpArchSwitchContext(ApplicationContext);

    PLOADER_PARAMETER_BLOCK loaderBlock = reinterpret_cast<PLOADER_PARAMETER_BLOCK>(resolvedAddress);
    UINT32 majorVersion = loaderBlock->OsMajorVersion;
    UINT32 minorVersion = loaderBlock->OsMinorVersion;
    UINT32 size = loaderBlock->Size;
    ContextPrint(EW(L"LOADER_PARAMETER_BLOCK    -> OsMajorVersion: %d OsMinorVersion: %d Size: %d\n"), majorVersion, minorVersion, size);

    auto kernelModule = Utils::GetModule(&loaderBlock->LoadOrderListHead, EW(L"ntoskrnl.exe"));
    if (!kernelModule.DllBase)
    {
        ContextPrint(EW(L"Failed to find ntoskrnl.exe in OslLoaderBlock!\n"));
        INFINITE_LOOP();
    }
    ContextPrint(EW(L"ntoskrnl.exe              -> (virt) 0x%p\n"), kernelModule.DllBase);
    
    UINT64 loadDriverScan = Utils::FindPatternImage(kernelModule.DllBase, E("E8 ? ? ? ? 33 D2 8B D8 44 8B FA"));
    if (!loadDriverScan)
    {
        ContextPrint(EW(L"Failed to find reference to IopLoadDriver!\n"));
        INFINITE_LOOP();
    }
    resolvedAddress = (loadDriverScan + 5) + *reinterpret_cast<int*>(loadDriverScan + 1);

    /*
     * We can't perform the hook here since we don't have the virtual address
     * mapping yet
     */
    Hooks::function = reinterpret_cast<Hooks::IopLoadDriver_t>(resolvedAddress);
    Hooks::kernelBase = kernelModule.DllBase;
    ContextPrint(EW(L"IopLoadDriver             -> (virt) 0x%p\n"), Hooks::function);

    /*
     * Switch back to firmware context before calling real ExitBootServices
     */
    BlpArchSwitchContext(FirmwareContext);

    /*
     * Wait so the text on screen is visible
     */
    gBS->Stall(SEC_TO_MICRO(1));

    /*
     * Call the original
     */
    return originalExitBootServices(ImageHandle, MapKey);
}

EXTERN_C EFI_STATUS EFIAPI UefiUnload(EFI_HANDLE ImageHandle)
{
    UNREFERENCED_PARAMETER(ImageHandle);
    return EFI_ACCESS_DENIED;
}

EXTERN_C EFI_STATUS EFIAPI UefiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
    UNREFERENCED_PARAMETER(ImageHandle);
    UNREFERENCED_PARAMETER(SystemTable);

    Console::Startup();

    originalExitBootServices = gBS->ExitBootServices;
    gBS->ExitBootServices = HookedExitBootServices;

    EFI_STATUS status = gBS->CreateEvent(EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE, TPL_NOTIFY, NotifySetVirtualAddressMap, NULL, &virtualEvent);
    ASSERT_EFI_ERROR(status);

    return EFI_SUCCESS;
}