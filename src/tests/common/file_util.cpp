// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <romx/romx.h>

#include "common/file_util.h"
#include "common/string_util.h"
#include "core/loader/loader.h"

TEST_CASE("SplitFilename83 Sanity", "[common]") {
    std::string filename = "long_ass_file_name.cci";
    std::array<char, 9> short_name;
    std::array<char, 4> extension;

    FileUtil::SplitFilename83(filename, short_name, extension);

    filename = Common::ToUpper(filename);
    std::string expected_short_name = filename.substr(0, 6).append("~1");
    std::string expected_extension = filename.substr(filename.find('.') + 1, 3);

    REQUIRE(std::memcmp(short_name.data(), expected_short_name.data(), short_name.size()) == 0);
    REQUIRE(std::memcmp(extension.data(), expected_extension.data(), extension.size()) == 0);
}

#if defined(__APPLE__)

TEST_CASE("NormalizeNFDToNFC Sanity", "[common]") {
    const std::string decomposed = "i\xCC\x81";
    const std::string composed = "\xC3\xAD";

    REQUIRE(Common::NormalizeNFDToNFC(decomposed) == composed);
}
#endif

TEST_CASE("IOFile exposes ROMX payload view", "[common][romx]") {
    const auto root = std::filesystem::temp_directory_path() /
                      "azahar-romx-iofile-test";
    const auto payload_path = root / "payload.cci";
    const auto metadata_path = root / "metadata.json";
    const auto romx_path = root / "payload.ccix";
    std::vector<u8> payload(0x200, 0);
    payload[0x100] = 'N';
    payload[0x101] = 'C';
    payload[0x102] = 'S';
    payload[0x103] = 'D';

    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    std::filesystem::create_directories(root);

    {
        std::ofstream payload_file(payload_path, std::ios::binary);
        payload_file.write(reinterpret_cast<const char*>(payload.data()),
                            static_cast<std::streamsize>(payload.size()));
        REQUIRE(payload_file.good());
    }
    {
        std::ofstream metadata_file(metadata_path, std::ios::binary);
        metadata_file << R"({"schema_version":"0.1.0","name":"IOFile test","platform":"3ds","payload_format":"cci"})";
        REQUIRE(metadata_file.good());
    }

    romx_writer_options_t options = ROMX_WRITER_OPTIONS_INIT;
    romx_writer_report_t report = ROMX_WRITER_REPORT_INIT;
    romx_error_t error{};
    REQUIRE(romx_writer_write_paths(
                romx_path.c_str(), payload_path.c_str(), metadata_path.c_str(), nullptr,
                &options, &report, &error) == ROMX_OK);
    REQUIRE(report.payload_size == payload.size());

    FileUtil::IOFile file(romx_path.string(), "rb");
    REQUIRE(file.IsOpen());
    REQUIRE(file.IsRomx());
    REQUIRE(file.GetSize() == payload.size());
    REQUIRE(file.Filename() == romx_path.string());
    REQUIRE(Loader::IdentifyFile(romx_path.string()) == Loader::FileType::CCI);

    std::vector<u8> sequential(payload.size());
    REQUIRE(file.ReadBytes(sequential.data(), sequential.size()) == sequential.size());
    REQUIRE(std::memcmp(sequential.data(), payload.data(), payload.size()) == 0);

    std::array<u8, 4> tail{};
    REQUIRE(file.ReadAtBytes(tail.data(), tail.size(), payload.size() - tail.size()) ==
            tail.size());
    REQUIRE(std::memcmp(tail.data(), payload.data() + payload.size() - tail.size(), tail.size()) ==
            0);

    REQUIRE(file.Seek(-4, SEEK_END));
    REQUIRE(file.Tell() == payload.size() - 4);
    REQUIRE(file.ReadBytes(tail.data(), tail.size()) == tail.size());
    REQUIRE(file.Tell() == payload.size());

    file.Clear();
    REQUIRE(file.Tell() == 0);

    file.Close();
    std::filesystem::remove_all(root, cleanup_error);
}
