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

        /// Scan the uploaded data directory for known expansion ESMs and, if
        /// found, uncomment the corresponding entries in the auto-generated
        /// openmw.cfg so that they are loaded on the next page reload.
        void updateCfgWithExpansions();
    }
}

#endif

#endif
