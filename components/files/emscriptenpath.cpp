#include "emscriptenpath.hpp"

#ifdef __EMSCRIPTEN__

#include <cstdlib>

namespace
{
    std::filesystem::path getEnvPath(const char* envVariable, const std::filesystem::path& fallback)
    {
        const char* result = std::getenv(envVariable);
        if (result == nullptr || result[0] == '\0')
            return fallback;
        return result;
    }
}

namespace Files
{
    EmscriptenPath::EmscriptenPath(const std::string& applicationName)
        : mName(applicationName)
    {
    }

    std::filesystem::path EmscriptenPath::getUserConfigPath() const
    {
        return getEnvPath("XDG_CONFIG_HOME", "/persistent/home/.config") / mName;
    }

    std::filesystem::path EmscriptenPath::getUserDataPath() const
    {
        return getEnvPath("XDG_DATA_HOME", "/persistent/home/.local/share") / mName;
    }

    std::filesystem::path EmscriptenPath::getCachePath() const
    {
        return std::filesystem::path("/tmp") / mName;
    }

    std::filesystem::path EmscriptenPath::getGlobalConfigPath() const
    {
        return std::filesystem::path("/config") / mName;
    }

    std::filesystem::path EmscriptenPath::getLocalPath() const
    {
        return std::filesystem::current_path() / "";
    }

    std::filesystem::path EmscriptenPath::getGlobalDataPath() const
    {
        return std::filesystem::path("/gamedata") / mName;
    }

    std::vector<std::filesystem::path> EmscriptenPath::getInstallPaths() const
    {
        return {};
    }
}

#endif
