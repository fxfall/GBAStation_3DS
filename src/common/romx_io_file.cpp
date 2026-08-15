// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/romx_io_file.h"

#include <limits>

#include "common/string_util.h"

namespace FileUtil {

namespace {

bool IsRomxPath(const std::string& path) {
    return Common::ToLower(std::string(GetExtensionFromFilename(path))) == "romx";
}

void CloseReader(romx_reader_t*& reader, romx_vfs_file_t*& entrypoint) {
    if (entrypoint != nullptr) {
        romx_vfs_file_close(entrypoint);
        entrypoint = nullptr;
    }
    if (reader != nullptr) {
        romx_reader_close(reader);
        reader = nullptr;
    }
}

} // namespace

RomxIOFile::RomxIOFile(const std::string& path_) : IOFile(), path(path_) {
    romx_error_t error{};
    romx_reader_options_t options = ROMX_READER_OPTIONS_INIT;

    if (romx_reader_open_path(path.c_str(), &options, &reader, &error) != ROMX_OK) {
        CloseReader(reader, entrypoint);
        return;
    }
    if (romx_reader_get_entrypoint(reader, &entry, &error) != ROMX_OK) {
        CloseReader(reader, entrypoint);
        return;
    }
    if (romx_vfs_file_open_entrypoint(reader, &entrypoint, &error) != ROMX_OK) {
        CloseReader(reader, entrypoint);
        return;
    }
    if (romx_vfs_file_get_size(entrypoint, &size, &error) != ROMX_OK) {
        CloseReader(reader, entrypoint);
        return;
    }

    open = true;
    good = true;
}

RomxIOFile::~RomxIOFile() {
    Close();
}

bool RomxIOFile::Close() {
    CloseReader(reader, entrypoint);
    open = false;
    return true;
}

bool RomxIOFile::IsOpen() const {
    return open && reader != nullptr && entrypoint != nullptr;
}

bool RomxIOFile::IsGood() const {
    return good && IsOpen();
}

int RomxIOFile::GetFd() const {
    return -1;
}

u64 RomxIOFile::GetSize() const {
    return size;
}

bool RomxIOFile::Resize(u64) {
    good = false;
    return false;
}

bool RomxIOFile::Flush() {
    return IsGood();
}

void RomxIOFile::Clear() {
    good = true;
}

bool RomxIOFile::IsCrypto() {
    return false;
}

bool RomxIOFile::IsCompressed() {
    return false;
}

const std::string& RomxIOFile::Filename() const {
    return path;
}

std::size_t RomxIOFile::ReadImpl(void* data, std::size_t length, std::size_t data_size) {
    if (!IsGood() || data_size == 0 ||
        length > std::numeric_limits<std::size_t>::max() / data_size) {
        good = false;
        return std::numeric_limits<std::size_t>::max();
    }

    const u64 byte_count = static_cast<u64>(length * data_size);
    u64 bytes_read = 0;
    romx_error_t error{};
    const romx_result_t result =
        romx_vfs_file_read(entrypoint, data, byte_count, &bytes_read, &error);
    if (result != ROMX_OK) {
        good = false;
        return std::numeric_limits<std::size_t>::max();
    }
    if (bytes_read != byte_count) {
        good = false;
    }
    return static_cast<std::size_t>(bytes_read / data_size);
}

std::size_t RomxIOFile::ReadAtImpl(void* data, std::size_t byte_count, std::size_t offset) {
    if (!IsGood()) {
        good = false;
        return std::numeric_limits<std::size_t>::max();
    }

    u64 bytes_read = 0;
    romx_error_t error{};
    const romx_result_t result = romx_reader_read_entry(
        reader, entry.index, static_cast<u64>(offset), data, static_cast<u64>(byte_count),
        &bytes_read, &error);
    if (result != ROMX_OK) {
        good = false;
        return std::numeric_limits<std::size_t>::max();
    }
    if (bytes_read != byte_count) {
        good = false;
    }
    return static_cast<std::size_t>(bytes_read);
}

std::size_t RomxIOFile::WriteImpl(const void*, std::size_t, std::size_t) {
    good = false;
    return std::numeric_limits<std::size_t>::max();
}

bool RomxIOFile::SeekImpl(s64 off, int origin) {
    if (!IsGood()) {
        good = false;
        return false;
    }

    romx_payload_seek_position_t position;
    switch (origin) {
    case SEEK_SET:
        position = ROMX_PAYLOAD_SEEK_START;
        break;
    case SEEK_CUR:
        position = ROMX_PAYLOAD_SEEK_CURRENT;
        break;
    case SEEK_END:
        position = ROMX_PAYLOAD_SEEK_END;
        break;
    default:
        good = false;
        return false;
    }

    romx_error_t error{};
    const bool result =
        romx_vfs_file_seek(entrypoint, static_cast<int64_t>(off), position, nullptr, &error) ==
        ROMX_OK;
    if (!result) {
        good = false;
    }
    return result;
}

u64 RomxIOFile::TellImpl() const {
    if (!IsGood()) {
        return std::numeric_limits<u64>::max();
    }

    u64 position = 0;
    romx_error_t error{};
    if (romx_vfs_file_tell(entrypoint, &position, &error) != ROMX_OK) {
        return std::numeric_limits<u64>::max();
    }
    return position;
}

std::unique_ptr<IOFile> OpenContentFile(const std::string& path, const char openmode[]) {
    const std::string mode(openmode == nullptr ? "rb" : openmode);
    const bool read_only = mode.find_first_of("wa+") == std::string::npos;
    if (read_only && IsRomxPath(path)) {
        return std::make_unique<RomxIOFile>(path);
    }
    return std::make_unique<IOFile>(path, mode.c_str());
}

} // namespace FileUtil
