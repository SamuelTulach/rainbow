#pragma once

namespace Utils
{
    UINT64 FindPattern(VOID* baseAddress, UINT64 size, const CHAR8* pattern);
    UINT64 FindPatternImage(VOID* base, const CHAR8* pattern);
    UINT64 GetExport(VOID* base, const CHAR8* functionName);
    KLDR_DATA_TABLE_ENTRY GetModule(LIST_ENTRY* list, const CHAR16* name);
    VOID CopyToReadOnly(VOID* destination, VOID* source, UINTN size);
    VOID CopyMemory(VOID* destination, VOID* source, UINTN size);
    VOID SetMemory(VOID* destination, UINT8 value, UINTN size);
}