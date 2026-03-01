#ifndef OPENMW_WASMFILEPICKER_H
#define OPENMW_WASMFILEPICKER_H

#ifdef __EMSCRIPTEN__

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

        /// Returns the total number of files discovered during directory scan.
        int getTotalFileCount();

        /// Returns the number of files uploaded so far.
        int getUploadedFileCount();

        /// Returns true if the essential files (ESM/ESP/BSA) have been loaded.
        /// Non-essential files may still be loading in the background.
        bool areEssentialFilesReady();
    }
}

#endif

#endif
