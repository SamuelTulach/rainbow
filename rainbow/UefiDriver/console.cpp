#include "general.h"

UINTN Console::IntToColor(UINTN input)
{
    switch (input)
    {
    case 0:
        return EFI_LIGHTBLUE;
    case 1:
        return EFI_LIGHTGREEN;
    case 2:
        return EFI_LIGHTCYAN;
    case 3:
        return EFI_LIGHTRED;
    case 4:
        return EFI_LIGHTMAGENTA;
    case 5:
        return EFI_YELLOW;
    case 6:
        return EFI_LIGHTBLUE;
    default:
        return EFI_BLUE;
    }
}

void Console::ClearScreen()
{
    gST->ConOut->SetAttribute(gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK);
    gST->ConOut->ClearScreen(gST->ConOut);
    Print(EW(L"\n"));
}

void Console::PrintTitle()
{
    auto* text = EW(L"rainbow");
    for (INTN i = 0; i < 7; i++)
    {
        gST->ConOut->SetAttribute(gST->ConOut, IntToColor(i) | EFI_BACKGROUND_BLACK);
        Print(EW(L"%c"), text[i]);
    }
    gST->ConOut->SetAttribute(gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK);
    Print(EW(L"\nbuild on %a\n\n"), __DATE__);

    Print(EW(L"For updates visit rainbow.tulach.cc (official project website).\n"));

    gST->ConOut->SetAttribute(gST->ConOut, EFI_RED | EFI_BACKGROUND_BLACK);
    Print(EW(L"Use this at your own risk. This is pre-release version.\n\n"));

    gST->ConOut->SetAttribute(gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK);
    gBS->Stall(SEC_TO_MICRO(3));
}

void Console::Startup()
{
    ClearScreen();
    PrintTitle();
}