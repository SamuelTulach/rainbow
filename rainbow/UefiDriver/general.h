#pragma once

#define EXTERN_C extern "C"
#define UNREFERENCED_PARAMETER(P) (P)

#define BIT32_MAX 0xffffffff
#define BIT64_MAX 0xffffffffffffffff
#define SCAN_MAX 0x5f5e100

#define INFINITE_LOOP() \
    while (TRUE) \
    { \
    }

#define SEC_TO_MICRO(x) ((UINTN)(x) * 1000 * 1000)

EXTERN_C
{
    /*
     * All EDK2 includes are made for C so we need to use
     * extern C with them
     */
    #include <Uefi.h>
    #include <Library/UefiLib.h>
    #include <Library/UefiBootServicesTableLib.h>
    #include <Library/UefiRuntimeServicesTableLib.h>
    #include <Library/UefiRuntimeLib.h>
    #include <Library/DebugLib.h>
    #include <Library/BaseLib.h>
    #include <Library/BaseMemoryLib.h>
    #include <Protocol/LoadedImage.h>
    #include <Library/DevicePathLib.h>

    void _disable(void);
    void _enable(void);
    unsigned __int64 __readcr0(void);
    void __writecr0(unsigned __int64 data);
}

/*
 * Those headers require C++ and CAN NOT be used with pure C
 */
#include "console.h"
#include "xor.h"
#include "ntdef.h"
#include "utils.h"
#include "hooks.h"