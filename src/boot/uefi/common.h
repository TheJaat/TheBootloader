/* common.h - what both UEFI loaders need: the x86_64 one (main.c) and
 * the AArch64 one (aarch64.c).
 *
 * Each loader is a single C file, so these are definitions, not
 * declarations: the file that includes this gets its own copy. */
#ifndef __UEFI_COMMON_H__
#define __UEFI_COMMON_H__

#include <stdint.h>
#include <stddef.h>

#include "efi.h"

static EFI_SYSTEM_TABLE  *ST;
static EFI_BOOT_SERVICES *BS;
static EFI_HANDLE         Image;

/* ---- what a freestanding C program must supply itself --------------- */

/* The compiler may turn struct copies and zeroing loops into calls to
 * these, even with -ffreestanding, so they must exist. */
void *memset(void *Dest, int Value, size_t Count)
{
    uint8_t *D = Dest;
    while (Count--) *D++ = (uint8_t)Value;
    return Dest;
}

void *memcpy(void *Dest, const void *Source, size_t Count)
{
    uint8_t *D = Dest;
    const uint8_t *S = Source;
    while (Count--) *D++ = *S++;
    return Dest;
}

static int GuidEqual(const EFI_GUID *A, const EFI_GUID *B)
{
    const uint8_t *X = (const uint8_t *)A, *Y = (const uint8_t *)B;
    for (size_t i = 0; i < sizeof(EFI_GUID); i++) {
        if (X[i] != Y[i]) return 0;
    }
    return 1;
}

/* ---- console ---------------------------------------------------------- */

static void Print(const CHAR16 *Text)
{
    ST->ConOut->OutputString(ST->ConOut, (CHAR16 *)Text);
}

static void PrintHex(uint64_t Value)
{
    CHAR16 Buffer[19];
    int i = 18;
    Buffer[i] = 0;
    do {
        Buffer[--i] = L"0123456789abcdef"[Value & 0xF];
        Value >>= 4;
    } while (Value && i > 2);
    Buffer[--i] = L'x';
    Buffer[--i] = L'0';
    Print(&Buffer[i]);
}

static void PrintDecimal(uint64_t Value)
{
    CHAR16 Buffer[21];
    int i = 20;
    Buffer[i] = 0;
    do {
        Buffer[--i] = (CHAR16)(L'0' + Value % 10);
        Value /= 10;
    } while (Value);
    Print(&Buffer[i]);
}

/* Reports a fatal error and gives control back to the firmware, which
 * shows its boot menu or tries the next boot option. Only usable before
 * ExitBootServices. */
#define FAIL(Message, Status) \
    do { Fail(Message, Status); return Status; } while (0)

static void Fail(const CHAR16 *Message, EFI_STATUS Status)
{
    Print(L"TheTaaJBoot: ");
    Print(Message);
    Print(L" (status ");
    PrintHex(Status);
    Print(L")\r\n");
    BS->Stall(5 * 1000 * 1000);   /* long enough to read it */
}

/* ---- files ------------------------------------------------------------ */

/* Reads the whole of the file at `Path` into memory from the pool. */
static EFI_STATUS ReadFile(const CHAR16 *Path, void **Buffer, UINTN *Size)
{
    EFI_GUID LoadedImageGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_GUID FileSystemGuid  = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *Loaded;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_PROTOCOL *Root, *File;
    EFI_STATUS Status;

    /* On the partition this loader itself came from. */
    Status = BS->HandleProtocol(Image, &LoadedImageGuid, (void **)&Loaded);
    if (EFI_ERROR(Status)) return Status;
    Status = BS->HandleProtocol(Loaded->DeviceHandle, &FileSystemGuid, (void **)&FileSystem);
    if (EFI_ERROR(Status)) return Status;
    Status = FileSystem->OpenVolume(FileSystem, &Root);
    if (EFI_ERROR(Status)) return Status;

    Status = Root->Open(Root, &File, (CHAR16 *)Path, EFI_FILE_MODE_READ, 0);
    Root->Close(Root);
    if (EFI_ERROR(Status)) return Status;

    /* The file size, by seeking to the end: simpler than GetInfo. */
    uint64_t End;
    File->SetPosition(File, ~0ULL);
    File->GetPosition(File, &End);
    File->SetPosition(File, 0);

    Status = BS->AllocatePool(EfiLoaderData, End, Buffer);
    if (!EFI_ERROR(Status)) {
        *Size = End;
        Status = File->Read(File, Size, *Buffer);
        if (!EFI_ERROR(Status) && *Size != End) Status = EFI_LOAD_ERROR;
    }
    File->Close(File);
    return Status;
}

#endif /* __UEFI_COMMON_H__ */
