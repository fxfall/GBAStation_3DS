// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include <romx/romx.h>

#include "common/file_util.h"

namespace FileUtil {

/// A read-only IOFile view over the entrypoint payload in a ROMX container.
///
/// The ROMX container is kept open by this object, but callers still see only
/// the selected payload: RIDX, metadata, cover, footer, and mutable bytes are
/// never exposed through the file interface.
class RomxIOFile final : public IOFile {
public:
    explicit RomxIOFile(const std::string& path);
    ~RomxIOFile() override;

    bool Close() override;
    [[nodiscard]] bool IsOpen() const override;
    [[nodiscard]] bool IsGood() const override;
    [[nodiscard]] int GetFd() const override;
    [[nodiscard]] u64 GetSize() const override;
    bool Resize(u64 size) override;
    bool Flush() override;
    void Clear() override;
    bool IsCrypto() override;
    bool IsCompressed() override;
    const std::string& Filename() const override;

private:
    std::size_t ReadImpl(void* data, std::size_t length, std::size_t data_size) override;
    std::size_t ReadAtImpl(void* data, std::size_t byte_count, std::size_t offset) override;
    std::size_t WriteImpl(const void* data, std::size_t length, std::size_t data_size) override;

    bool SeekImpl(s64 off, int origin) override;
    u64 TellImpl() const override;

    romx_reader_t* reader = nullptr;
    romx_vfs_file_t* entrypoint = nullptr;
    romx_entry_info_t entry = ROMX_ENTRY_INFO_INIT;
    std::string path;
    u64 size = 0;
    bool open = false;
    bool good = false;
};

/// Open a normal file or, for a .romx path, a fresh entrypoint payload view.
/// Every call creates an independent ROMX reader/cursor.
[[nodiscard]] std::unique_ptr<IOFile> OpenContentFile(const std::string& path,
                                                       const char openmode[] = "rb");

} // namespace FileUtil
