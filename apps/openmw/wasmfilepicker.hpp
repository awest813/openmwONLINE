#ifndef OPENMW_WASMFILEPICKER_H
#define OPENMW_WASMFILEPICKER_H

#ifdef __EMSCRIPTEN__

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace OMW
{
    namespace WasmFilePicker
    {
        void initialize(const std::filesystem::path& dataMount);

        bool isDataReady();

        const std::filesystem::path& getDataPath();

        std::vector<std::string> listUploadedFiles();

        void registerBrowserCallbacks();

        uint32_t getUploadedFileCount();
        uint64_t getUploadedByteCount();
    }
}

#endif

#endif
