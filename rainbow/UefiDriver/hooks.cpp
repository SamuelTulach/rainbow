#include "general.h"

void Hooks::Hook()
{
    Utils::CopyToReadOnly(function, newData, 15);
}

void Hooks::Unhook()
{
    Utils::CopyToReadOnly(function, originalData, 15);
}

VOID* Hooks::GetModuleBase(const CHAR16* moduleName)
{
    static LIST_ENTRY* PsLoadedModuleList = nullptr;
    if (!PsLoadedModuleList)
        PsLoadedModuleList = *reinterpret_cast<LIST_ENTRY**>(Utils::GetExport(kernelBase, E("PsLoadedModuleList")));

    auto moduleInfo = Utils::GetModule(PsLoadedModuleList, moduleName);
    return moduleInfo.DllBase;
}

EFI_STATUS Hooks::ChangeDiskDispatch()
{
    VOID* base = GetModuleBase(EW(L"CLASSPNP.SYS"));
    if (!base)
        return EFI_NOT_FOUND;

    /*
     * ClassMpdevInternalDeviceControl
     * It will instantly return with unsuccessful status
     */
    UINT64 scan = Utils::FindPatternImage(base, E("40 53 48 83 EC 20 48 8B 41 40 48 8B DA 4C 8B C1 80 B8 ? ? ? ? ? 74 57"));
    if (!scan)
        return EFI_NOT_FOUND;

    static RtlInitUnicodeString_t RtlInitUnicodeString = nullptr;
    if (!RtlInitUnicodeString)
        RtlInitUnicodeString = reinterpret_cast<RtlInitUnicodeString_t>(Utils::GetExport(kernelBase, E("RtlInitUnicodeString")));

    UNICODE_STRING targetName;
    RtlInitUnicodeString(&targetName, EW(L"\\Driver\\Disk"));

    static ObReferenceObjectByName_t ObReferenceObjectByName = nullptr;
    if (!ObReferenceObjectByName)
        ObReferenceObjectByName = reinterpret_cast<ObReferenceObjectByName_t>(Utils::GetExport(kernelBase, E("ObReferenceObjectByName")));

    static POBJECT_TYPE* IoDriverObjectType = nullptr;
    if (!IoDriverObjectType)
        IoDriverObjectType = reinterpret_cast<POBJECT_TYPE*>(Utils::GetExport(kernelBase, E("IoDriverObjectType")));

    PDRIVER_OBJECT driverObject;
    auto status = ObReferenceObjectByName(&targetName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, 0, *IoDriverObjectType, KernelMode, nullptr, reinterpret_cast<VOID**>(&driverObject));
    if (status != 0)
        return EFI_NOT_READY;

    *reinterpret_cast<UINT64*>(&driverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]) = scan;

    static ObfDereferenceObject_t ObfDereferenceObject = nullptr;
    if (!ObfDereferenceObject)
        ObfDereferenceObject = reinterpret_cast<ObfDereferenceObject_t>(Utils::GetExport(kernelBase, E("ObfDereferenceObject")));

    ObfDereferenceObject(driverObject);
    return EFI_SUCCESS;
}

EFI_STATUS Hooks::ChangeNetworkDispatch()
{
    VOID* base = GetModuleBase(EW(L"ndis.sys"));
    if (!base)
        return EFI_NOT_FOUND;

    /*
     * ndisDummyIrpHandler
     * M$ was so kind to have this function in there
     */
    UINT64 scan = Utils::FindPatternImage(base, E("48 8D 05 ? ? ? ? B9 ? ? ? ? 49 8D 7E 70"));
    if (!scan)
        return EFI_NOT_FOUND;
    scan = reinterpret_cast<UINT64>(RELATIVE_ADDRESS((UINT8*)scan, 7));

    /*
     * _NDIS_M_DRIVER_BLOCK* ndisMiniDriverList
     */
    UINT64 listScan = Utils::FindPatternImage(base, E("48 8B 35 ? ? ? ? 44 0F B6 E0"));
    if (!listScan)
        return EFI_NOT_FOUND;

    bool found = false;
    PNDIS_M_DRIVER_BLOCK block = *static_cast<PNDIS_M_DRIVER_BLOCK*>(RELATIVE_ADDRESS(listScan, 7));
    for (PNDIS_M_DRIVER_BLOCK currentDriver = block; currentDriver; currentDriver = static_cast<PNDIS_M_DRIVER_BLOCK>(currentDriver->NextDriver))
    {
        if (!currentDriver->DriverObject)
            continue;

        if (!currentDriver->DriverObject->MajorFunction)
            continue;

        *reinterpret_cast<UINT64*>(&currentDriver->DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]) = scan;
        found = true;
    }

    return found ? EFI_SUCCESS : EFI_NOT_FOUND;
}

EFI_STATUS Hooks::ZeroSmbiosData()
{
    /*
     * WmipFindSMBiosStructure -> WmipSMBiosTablePhysicalAddress
     */
    auto* physicalAddress = reinterpret_cast<LARGE_INTEGER*>(Utils::FindPatternImage(kernelBase, E("48 8B 0D ? ? ? ? 48 85 C9 74 ? 8B 15")));
    if (!physicalAddress)
        return EFI_NOT_FOUND;

    physicalAddress = reinterpret_cast<LARGE_INTEGER*>(reinterpret_cast<char*>(physicalAddress) + 7 + *reinterpret_cast<int*>(reinterpret_cast<char*>(physicalAddress) + 3));
    if (!physicalAddress)
        return EFI_NOT_FOUND;

    Utils::SetMemory(physicalAddress, 0, sizeof(LARGE_INTEGER));

    return EFI_SUCCESS;
}

INTN Hooks::HookedIopLoadDriver(VOID* KeyHandle)
{
    /*
     * I originally did not want to release this source
     * for one simple reason and that is this "clever DKOM" (kek)
     * I have mentioned. All of this spoofing work in incredibly
     * primitive way.
     *
     * We take the dispatch of given driver (let's say disk.sys)
     * and we simply replace its device control function in its
     * DriverObject.
     *
     * Normally this would be a big red flag so how is that it was
     * not getting people instantly banned?
     *
     * Anticheats do a very simple checks on those dispatches. If
     * it points to a legit driver, it basically means it valid.
     * If you can get it to point to the driver it's actually intended
     * to point in, then congratulations, you won!
     *
     * How can we manage to do such a thing without patching the target
     * driver itself? Well... We can just choose any function that will
     * instantly return! When any software then tries to access the
     * driver's functionality, it will just instantly error out with no data
     * output.
     *
     * In our example of disk.sys, we will point it to ClassMpdevInternalDeviceControl
     * which (as I said before) will immediately return and error out.
     * The system will then think there are not drives attached.
     * 
     * To actually detect this, you would have to know where the original function
     * is supposed to be pointing or just start kicking people who's dispatch
     * errors out (because it's basically impossible for it to fail in normal conditions).
     */

    /*
     * Unhook so we can call original
     */
    Unhook();

    /*
     * Call original
     */
    INTN status = function(KeyHandle);

    /*
     * Hook the function back so this will get called again
     */
    Hook();

    /*
     * We don't care if the driver load was unsuccessful
     */
    if (status != 0)
        return status;

    /*
     * Resolve DbgPrintEx for possible debugging
     */
    static DbgPrintEx_t DbgPrintEx = nullptr;
    if (!DbgPrintEx)
        DbgPrintEx = reinterpret_cast<DbgPrintEx_t>(Utils::GetExport(kernelBase, E("DbgPrintEx")));

    /*
     * When everything is done, we will stop hooking
     * and exit
     */
    static bool diskDone = false;
    static bool nicDone = false;
    static bool smbiosDone = false;

    /*
     * Change disk dispatch
     */
    if (!diskDone && ChangeDiskDispatch() == EFI_SUCCESS)
        diskDone = true;

    /*
     * Change dispatch of every network adapter
     */
    if (!nicDone && ChangeNetworkDispatch() == EFI_SUCCESS)
        nicDone = true;

    /*
     * Zero out the address of SMBIOS structure
     */
    if (!smbiosDone && ZeroSmbiosData() == EFI_SUCCESS)
        smbiosDone = true;

    /*
     * Everything is done so do final unhook and bye bye
     */
    if (diskDone && nicDone && smbiosDone)
    {
        /*
         * Oh hey! You actually reading the code?
         * You might want to delete this print...
         */
        DbgPrintEx(0, 0, "[efi] All set, exiting\n");
        Unhook();
    }

    /*
     * Just a quick sidenote here, sometimes it randomly fails to unhook
     * and you will get patchguard crash (+ probably you will get flagged)
     * I am too lazy to figure out what is wrong, you can try that
     */

    return 0;
}
