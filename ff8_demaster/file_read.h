#pragma once
#include <windows.h>
#include <cstddef>
#include <memory>

// ============================================================
// A WHOLE FILE IN ONE READ
// LoadImageFromFile read every texture through std::fstream (seekg, read): the stream refills its small buffer over and
// over, 18 ms for a 21.3 MB DDS already in the system cache against 5.5 ms for one ReadFile of the same bytes
// (measured, x86). A battle stage reads 40 to 120 MB of them at its start.
// False (and nothing in `bytes`) when the file cannot be opened, is empty or cannot be read whole: LoadImageFromFile
// then returns no image, as it did when the stream failed or the parse of zero bytes did.
// ============================================================
inline bool ReadWholeFile(const char* path, std::unique_ptr<char[]>& bytes, size_t& size) {
    bytes.reset();
    size = 0;
    const HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                    FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file, &length) || length.QuadPart <= 0 || length.QuadPart > 0x7FFFFFFF) {
        CloseHandle(file);
        return false;
    }
    std::unique_ptr<char[]> buffer(new (std::nothrow) char[size_t(length.QuadPart)]);
    if (!buffer) {
        CloseHandle(file);
        return false;
    }
    size_t done = 0;
    const size_t total = size_t(length.QuadPart);
    while (done < total) {
        DWORD got = 0;
        if (!ReadFile(file, buffer.get() + done, DWORD(total - done), &got, nullptr) || !got) break;
        done += got;
    }
    CloseHandle(file);
    if (done != total) return false;
    bytes = std::move(buffer);
    size = total;
    return true;
}
