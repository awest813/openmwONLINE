#ifndef COMPONENTS_FILES_EMSCRIPTENPATH_H
#define COMPONENTS_FILES_EMSCRIPTENPATH_H

#ifdef __EMSCRIPTEN__

#include <filesystem>
#include <vector>

namespace Files
{
    struct EmscriptenPath
    {
        explicit EmscriptenPath(const std::string& applicationName);

        std::filesystem::path getUserConfigPath() const;

        std::filesystem::path getUserDataPath() const;

        std::filesystem::path getGlobalConfigPath() const;

        std::filesystem::path getLocalPath() const;

        std::filesystem::path getGlobalDataPath() const;

        std::filesystem::path getCachePath() const;

        std::vector<std::filesystem::path> getInstallPaths() const;

        std::string mName;
    };

}

#endif

#endif
