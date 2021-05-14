#include "general.h"

#define IN_RANGE(x, a, b) (x >= a && x <= b)
#define GET_BITS(x) (IN_RANGE((x&(~0x20)),'A','F')?((x&(~0x20))-'A'+0xA):(IN_RANGE(x,'0','9')?x-'0':0))
#define GET_BYTE(a, b) (GET_BITS(a) << 4 | GET_BITS(b))
UINT64 Utils::FindPattern(VOID* baseAddress, UINT64 size, const CHAR8* pattern)
{
    UINT8* firstMatch = nullptr;
    const CHAR8* currentPattern = pattern;

    UINT8* start = static_cast<UINT8*>(baseAddress);
    UINT8* end = start + size;

    for (UINT8* current = start; current < end; current++)
    {
        UINT8 byte = currentPattern[0]; if (!byte) return reinterpret_cast<UINT64>(firstMatch);
        if (byte == '\?' || *static_cast<UINT8*>(current) == GET_BYTE(byte, currentPattern[1]))
        {
            if (!firstMatch) firstMatch = current;
            if (!currentPattern[2]) return reinterpret_cast<UINT64>(firstMatch);
            ((byte == '\?') ? (currentPattern += 2) : (currentPattern += 3));
        }
        else
        {
            currentPattern = pattern;
            firstMatch = nullptr;
        }
    }

    return 0;
}

UINT64 Utils::FindPatternImage(VOID* base, const CHAR8* pattern)
{
    UINT64 match = 0;

    PIMAGE_NT_HEADERS64 headers = reinterpret_cast<PIMAGE_NT_HEADERS64>(reinterpret_cast<UINT64>(base) + static_cast<PIMAGE_DOS_HEADER>(base)->e_lfanew);
    PIMAGE_SECTION_HEADER sections = IMAGE_FIRST_SECTION(headers);
    for (INTN i = 0; i < headers->FileHeader.NumberOfSections; ++i)
    {
        PIMAGE_SECTION_HEADER section = &sections[i];
        if (*reinterpret_cast<UINT32*>(section->Name) == 'EGAP' || CompareMem(section->Name, ".text", 5) == 0)
        {
            match = FindPattern(reinterpret_cast<void*>(reinterpret_cast<UINT64>(base) + section->VirtualAddress), section->Misc.VirtualSize, pattern);
            if (match)
                break;
        }
    }

    return match;
}

UINT64 Utils::GetExport(VOID* base, const CHAR8* functionName)
{
    PIMAGE_DOS_HEADER dosHeader = static_cast<PIMAGE_DOS_HEADER>(base);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
        return 0;

    PIMAGE_NT_HEADERS64 ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS64>(reinterpret_cast<UINT64>(base) + dosHeader->e_lfanew);

    UINT32 exportsRva = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!exportsRva)
        return 0;

    PIMAGE_EXPORT_DIRECTORY exports = reinterpret_cast<PIMAGE_EXPORT_DIRECTORY>(reinterpret_cast<UINT64>(base) + exportsRva);
    UINT32* nameRva = reinterpret_cast<UINT32*>(reinterpret_cast<UINT64>(base) + exports->AddressOfNames);

    for (UINT32 i = 0; i < exports->NumberOfNames; ++i)
    {
        CHAR8* func = reinterpret_cast<CHAR8*>(reinterpret_cast<UINT64>(base) + nameRva[i]);
        if (AsciiStrCmp(func, functionName) == 0)
        {
            UINT32* funcRva = (UINT32*)(reinterpret_cast<UINT64>(base) + exports->AddressOfFunctions);
            UINT16* ordinalRva = (UINT16*)(reinterpret_cast<UINT64>(base) + exports->AddressOfNameOrdinals);

            return reinterpret_cast<UINT64>(base) + funcRva[ordinalRva[i]];
        }
    }

    return 0;
}

KLDR_DATA_TABLE_ENTRY Utils::GetModule(LIST_ENTRY* list, const CHAR16* name)
{
    for (LIST_ENTRY* entry = list->ForwardLink; entry != list; entry = entry->ForwardLink)
    {
        PKLDR_DATA_TABLE_ENTRY module = CONTAINING_RECORD(entry, KLDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        if (module && StrnCmp(name, module->BaseDllName.Buffer, module->BaseDllName.Length) == 0)
            return *module;
    }

    return {};
}

VOID Utils::CopyMemory(VOID* destination, VOID* source, UINTN size)
{
    /*
     * CopyMem is part of boot services (at least in EDK2 - why??)
     * memcpy? Inconsistent! Sometimes it gets inlined, sometimes
     * you will undefined reference
     */
    UINT8* dst = static_cast<UINT8*>(destination);
    UINT8* src = static_cast<UINT8*>(source);
    for (UINTN i = 0; i < size; i++)
    {
        dst[i] = src[i];
    }
}

VOID Utils::SetMemory(VOID* destination, UINT8 value, UINTN size)
{
    UINT8* dst = static_cast<UINT8*>(destination);
    for (UINTN i = 0; i < size; i++)
    {
        dst[i] = value;
    }
}

VOID Utils::CopyToReadOnly(VOID* destination, VOID* source, UINTN size)
{
    _disable();
    UINT64 cr0 = __readcr0();
    UINT64 oldCr0 = cr0;
    cr0 &= ~(1UL << 16);
    __writecr0(cr0);

    CopyMemory(destination, source, size);

    __writecr0(oldCr0);

    /*
     * Probably could check if interrupts were enabled in first
     * place but eh...
     */
    _enable();
}