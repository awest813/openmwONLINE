#include <components/debug/debugging.hpp>
#include <components/fallback/fallback.hpp>
#include <components/fallback/validate.hpp>
#include <components/files/configurationmanager.hpp>
#include <components/misc/osgpluginchecker.hpp>
#include <components/misc/rng.hpp>
#include <components/platform/platform.hpp>
#include <components/version/version.hpp>

#include "mwgui/debugwindow.hpp"

#include "engine.hpp"
#include "options.hpp"

#include <boost/program_options/variables_map.hpp>

#ifdef __EMSCRIPTEN__
#    include <emscripten.h>
#    include "wasmfilepicker.hpp"
#endif

#if defined(_WIN32)
#include <components/misc/windows.hpp>
// makes __argc and __argv available on windows
#include <cstdlib>

extern "C" __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;
#endif

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#if (defined(__APPLE__) || defined(__linux) || defined(__unix) || defined(__posix))
#include <unistd.h>
#endif

#if !defined(_WIN32)
#include <cstdlib>
#endif

/**
 * \brief Parses application command line and calls \ref Cfg::ConfigurationManager
 * to parse configuration files.
 *
 * Results are directly written to \ref Engine class.
 *
 * \retval true - Everything goes OK
 * \retval false - Error
 */
bool parseOptions(int argc, char** argv, OMW::Engine& engine, Files::ConfigurationManager& cfgMgr)
{
    // Create a local alias for brevity
    namespace bpo = boost::program_options;
    typedef std::vector<std::string> StringsVector;

    bpo::options_description desc = OpenMW::makeOptionsDescription();
    bpo::variables_map variables;

    Files::parseArgs(argc, argv, variables, desc);
    bpo::notify(variables);

    if (variables.count("help"))
    {
        Debug::getRawStdout() << desc << std::endl;
        return false;
    }

    if (variables.count("version"))
    {
        Debug::getRawStdout() << Version::getOpenmwVersionDescription() << std::endl;
        return false;
    }

    cfgMgr.processPaths(variables, std::filesystem::current_path());

    cfgMgr.readConfiguration(variables, desc);

    Debug::setupLogging(cfgMgr.getLogPath(), "OpenMW");
    Log(Debug::Info) << Version::getOpenmwVersionDescription();

    Settings::Manager::load(cfgMgr);

    MWGui::DebugWindow::startLogRecording();

    engine.setGrabMouse(!variables["no-grab"].as<bool>());

    // Font encoding settings
    std::string encoding(variables["encoding"].as<std::string>());
    Log(Debug::Info) << ToUTF8::encodingUsingMessage(encoding);
    engine.setEncoding(ToUTF8::calculateEncoding(encoding));

    Files::PathContainer dataDirs(asPathContainer(variables["data"].as<Files::MaybeQuotedPathContainer>()));

    Files::PathContainer::value_type local(variables["data-local"]
                                               .as<Files::MaybeQuotedPathContainer::value_type>()
                                               .u8string()); // This call to u8string is redundant, but required to
                                                             // build on MSVC 14.26 due to implementation bugs.
    if (!local.empty())
        dataDirs.push_back(std::move(local));

    cfgMgr.filterOutNonExistingPaths(dataDirs);

    engine.setResourceDir(variables["resources"]
                              .as<Files::MaybeQuotedPath>()
                              .u8string()); // This call to u8string is redundant, but required to build on MSVC 14.26
                                            // due to implementation bugs.
    engine.setDataDirs(dataDirs);

    // fallback archives
    StringsVector archives = variables["fallback-archive"].as<StringsVector>();
    for (const auto& archive : archives)
        engine.addArchive(archive);

    StringsVector content = variables["content"].as<StringsVector>();
    if (content.empty())
    {
        Log(Debug::Error) << "No content file given (esm/esp, nor omwgame/omwaddon). Aborting...";
        return false;
    }
    engine.addContentFile("builtin.omwscripts");
    std::set<std::string> contentDedupe{ "builtin.omwscripts" };
    for (const auto& contentFile : content)
    {
        if (!contentDedupe.insert(contentFile).second)
        {
            Log(Debug::Error) << "Content file specified more than once: " << contentFile << ". Aborting...";
            return false;
        }
    }

    for (auto& file : content)
    {
        engine.addContentFile(file);
    }

    StringsVector groundcover = variables["groundcover"].as<StringsVector>();
    for (auto& file : groundcover)
    {
        engine.addGroundcoverFile(file);
    }

    if (variables.count("lua-scripts"))
    {
        Log(Debug::Warning) << "Lua scripts have been specified via the old lua-scripts option and will not be loaded. "
                               "Please update them to a version which uses the new omwscripts format.";
    }

    // startup-settings
    engine.setCell(variables["start"].as<std::string>());
    engine.setSkipMenu(variables["skip-menu"].as<bool>(), variables["new-game"].as<bool>());
    if (!variables["skip-menu"].as<bool>() && variables["new-game"].as<bool>())
        Log(Debug::Warning) << "Warning: new-game used without skip-menu -> ignoring it";

    // scripts
    engine.setCompileAll(variables["script-all"].as<bool>());
    engine.setCompileAllDialogue(variables["script-all-dialogue"].as<bool>());
    engine.setScriptConsoleMode(variables["script-console"].as<bool>());
    engine.setStartupScript(variables["script-run"].as<std::string>());
    engine.setWarningsMode(variables["script-warn"].as<int>());
    engine.setSaveGameFile(variables["load-savegame"].as<Files::MaybeQuotedPath>().u8string());

    // other settings
    Fallback::Map::init(variables["fallback"].as<Fallback::FallbackMap>().mMap);
    engine.setSoundUsage(!variables["no-sound"].as<bool>());
    engine.setActivationDistanceOverride(variables["activate-dist"].as<int>());
    engine.enableFontExport(variables["export-fonts"].as<bool>());
    engine.setRandomSeed(variables["random-seed"].as<unsigned int>());

    return true;
}

namespace
{
#ifdef __EMSCRIPTEN__
    std::string getWasmPersistentRootPath()
    {
        static constexpr const char* sDefaultRoot = "/persistent";

        const char* rootEnv = std::getenv("OPENMW_WASM_PERSISTENT_ROOT");
        if (rootEnv == nullptr || rootEnv[0] == '\0')
            return sDefaultRoot;

        std::string rootPath(rootEnv);
        if (rootPath.front() != '/')
        {
            Log(Debug::Warning) << "Ignoring OPENMW_WASM_PERSISTENT_ROOT='" << rootPath
                                << "' because it is not an absolute path. Falling back to " << sDefaultRoot << ".";
            return sDefaultRoot;
        }

        while (rootPath.size() > 1 && rootPath.back() == '/')
            rootPath.pop_back();

        return rootPath;
    }

    std::string toJavaScriptStringLiteral(const std::string& input)
    {
        constexpr char hexDigits[] = "0123456789abcdef";
        std::ostringstream escaped;
        escaped << '"';
        for (unsigned char c : input)
        {
            switch (c)
            {
                case '\\': escaped << "\\\\"; break;
                case '"':  escaped << "\\\""; break;
                case '\n': escaped << "\\n";  break;
                case '\r': escaped << "\\r";  break;
                case '\t': escaped << "\\t";  break;
                case '\0': escaped << "\\0";  break;
                default:
                    if (c < 0x20)
                    {
                        // Escape remaining ASCII control characters as \uXXXX
                        escaped << "\\u00";
                        escaped << hexDigits[c >> 4];
                        escaped << hexDigits[c & 0xf];
                    }
                    else
                    {
                        escaped << static_cast<char>(c);
                    }
                    break;
            }
        }
        escaped << '"';
        return escaped.str();
    }

    int getWasmPersistentSyncIntervalMs()
    {
        static constexpr int sDefaultIntervalMs = 15000;
        static constexpr int sMinIntervalMs = 1000;
        static constexpr int sMaxIntervalMs = 300000;

        const char* intervalEnv = std::getenv("OPENMW_WASM_PERSISTENT_SYNC_INTERVAL_MS");
        if (intervalEnv == nullptr || intervalEnv[0] == '\0')
            return sDefaultIntervalMs;

        char* end = nullptr;
        errno = 0;
        const long parsedValue = std::strtol(intervalEnv, &end, 10);

        if (errno != 0 || end == intervalEnv || (end != nullptr && *end != '\0'))
        {
            Log(Debug::Warning) << "Ignoring invalid OPENMW_WASM_PERSISTENT_SYNC_INTERVAL_MS value: '" << intervalEnv
                                << "'. Falling back to " << sDefaultIntervalMs << " ms.";
            return sDefaultIntervalMs;
        }

        if (parsedValue <= 0)
        {
            Log(Debug::Info) << "OPENMW_WASM_PERSISTENT_SYNC_INTERVAL_MS<=0 disables periodic IDBFS sync timer.";
            return 0;
        }

        if (parsedValue < sMinIntervalMs)
        {
            Log(Debug::Warning) << "OPENMW_WASM_PERSISTENT_SYNC_INTERVAL_MS=" << parsedValue
                                << " is too small; clamping to " << sMinIntervalMs << " ms.";
            return sMinIntervalMs;
        }

        if (parsedValue > sMaxIntervalMs)
        {
            Log(Debug::Warning) << "OPENMW_WASM_PERSISTENT_SYNC_INTERVAL_MS=" << parsedValue
                                << " is too large; clamping to " << sMaxIntervalMs << " ms.";
            return sMaxIntervalMs;
        }

        return static_cast<int>(parsedValue);
    }

    void bootstrapWasmConfigFile()
    {
        // Ensure a minimal openmw.cfg exists so that the engine can find game data
        // uploaded via the browser file picker at /gamedata.
        const char* cfgHome = std::getenv("XDG_CONFIG_HOME");
        const std::string configBase = (cfgHome && cfgHome[0] != '\0') ? cfgHome : "/persistent/home/.config";
        const std::string cfgDir = configBase + "/openmw";
        const std::string cfgFile = cfgDir + "/openmw.cfg";

        // Only bootstrap if no config file exists yet (first run or fresh IDBFS)
        if (std::filesystem::exists(cfgFile))
        {
            Log(Debug::Info) << "WASM: Found existing config at " << cfgFile;
            return;
        }

        Log(Debug::Info) << "WASM: Bootstrapping default openmw.cfg at " << cfgFile;

        std::filesystem::create_directories(cfgDir);

        std::ofstream out(cfgFile);
        if (!out.is_open())
        {
            Log(Debug::Error) << "WASM: Failed to create default openmw.cfg at " << cfgFile;
            return;
        }

        out << "# Auto-generated default config for OpenMW WASM build\n"
            << "# Game data is loaded from /gamedata via the browser file picker\n"
            << "\n"
            << "data=/gamedata\n"
            << "\n"
            << "# Morrowind base content\n"
            << "content=Morrowind.esm\n"
            << "\n"
            << "# Base game archives\n"
            << "fallback-archive=Morrowind.bsa\n"
            << "\n"
            << "# Tribunal expansion (ignored if not present)\n"
            << "# content=Tribunal.esm\n"
            << "# fallback-archive=Tribunal.bsa\n"
            << "\n"
            << "# Bloodmoon expansion (ignored if not present)\n"
            << "# content=Bloodmoon.esm\n"
            << "# fallback-archive=Bloodmoon.bsa\n"
            << "\n"
            << "encoding=win1252\n";

        out.close();
        Log(Debug::Info) << "WASM: Default openmw.cfg written successfully";
    }

    void initializeWasmPersistentStorage()
    {
        const int periodicSyncIntervalMs = getWasmPersistentSyncIntervalMs();
        const std::string persistentRootPath = getWasmPersistentRootPath();
        const std::string homePath = persistentRootPath + "/home";
        const std::string configPath = homePath + "/.config";
        const std::string dataPath = homePath + "/.local/share";

#ifdef __EMSCRIPTEN_PTHREADS__
        emscripten_run_script(R"(
            if (typeof crossOriginIsolated !== 'undefined' && !crossOriginIsolated) {
                console.warn('OpenMW WASM was built with pthread support, but this page is not cross-origin isolated. '
                    + 'Set Cross-Origin-Opener-Policy: same-origin and '
                    + 'Cross-Origin-Embedder-Policy: require-corp to enable Web Worker threads.');
            }
        )");
#endif

        // The IDBFS mount and initial sync-from-storage are handled in the HTML
        // shell's Module.preRun callback using addRunDependency/removeRunDependency,
        // which guarantees that the persistent filesystem is fully populated BEFORE
        // main() runs.  Here we only need to:
        //   1. Create any XDG subdirectories that were not in the stored image
        //      (first run, or a new subdirectory added in a later build).
        //   2. Set HOME and XDG environment variables so OpenMW finds its paths.
        //   3. Register the periodic background sync and page-lifecycle hooks.
        std::string persistenceScript = R"(
            if (typeof FS === 'undefined' || typeof IDBFS === 'undefined') {
                console.error('Emscripten FS/IDBFS APIs are unavailable; persistent storage disabled.');
            } else {
                const persistentRoot = __OPENMW_PERSISTENT_ROOT__;
                const homeRoot = __OPENMW_HOME_PATH__;
                const configRoot = __OPENMW_CONFIG_PATH__;
                const localRoot = homeRoot + '/.local';
                const dataRoot = __OPENMW_DATA_PATH__;

                // Ensure subdirectories exist (safe no-op if already created by preRun sync).
                if (!FS.analyzePath(persistentRoot).exists)
                    FS.mkdir(persistentRoot);
                if (!FS.analyzePath(homeRoot).exists)
                    FS.mkdir(homeRoot);
                if (!FS.analyzePath(configRoot).exists)
                    FS.mkdir(configRoot);
                if (!FS.analyzePath(localRoot).exists)
                    FS.mkdir(localRoot);
                if (!FS.analyzePath(dataRoot).exists)
                    FS.mkdir(dataRoot);

                var syncPersistentStorage = function() {
                    var state = (typeof globalThis !== 'undefined')
                        ? (globalThis.__openmwPersistentSyncState = globalThis.__openmwPersistentSyncState || {})
                        : {};

                    if (state.syncInProgress) {
                        state.syncPending = true;
                        return;
                    }

                    state.syncInProgress = true;
                    FS.syncfs(false, function(error) {
                        state.syncInProgress = false;

                        if (error)
                            console.error('Background IDBFS sync failed', error);

                        if (state.syncPending) {
                            state.syncPending = false;
                            syncPersistentStorage();
                        }
                    });
                };

                var schedulePeriodicPersistentSync = function() {
                    var state = (typeof globalThis !== 'undefined')
                        ? (globalThis.__openmwPersistentSyncState = globalThis.__openmwPersistentSyncState || {})
                        : {};

                    if (state.periodicSyncTimer)
                        return;

                    var periodicSyncIntervalMs = __OPENMW_SYNC_INTERVAL_MS__;
                    if (periodicSyncIntervalMs <= 0)
                        return;

                    state.periodicSyncTimer = setInterval(function() {
                        syncPersistentStorage();
                    }, periodicSyncIntervalMs);
                };

                if (typeof globalThis !== 'undefined')
                    globalThis.__openmwSyncPersistentStorage = syncPersistentStorage;

                if (typeof window !== 'undefined' && !window.__openmwPersistentSyncRegistered) {
                    window.addEventListener('visibilitychange', function() {
                        if (document.visibilityState === 'hidden')
                            syncPersistentStorage();
                    });
                    window.addEventListener('pagehide', syncPersistentStorage);
                    window.addEventListener('beforeunload', syncPersistentStorage);
                    window.addEventListener('online', syncPersistentStorage);
                    window.__openmwPersistentSyncRegistered = true;
                }

                schedulePeriodicPersistentSync();
            }
        )";

        // Replace each placeholder token in the script template.  Guard against
        // missing tokens to avoid std::string::replace() throwing std::out_of_range
        // if the script template is accidentally edited.
        const auto replaceScriptToken = [](std::string& scriptTemplate, std::string_view token,
                                            std::string_view value) {
            const auto pos = scriptTemplate.find(token);
            if (pos == std::string::npos)
            {
                Log(Debug::Error) << "WASM: Missing template token '" << token
                                  << "' in persistence script — persistent storage may be misconfigured";
                return;
            }
            scriptTemplate.replace(pos, token.size(), value);
        };

        replaceScriptToken(persistenceScript, "__OPENMW_SYNC_INTERVAL_MS__", std::to_string(periodicSyncIntervalMs));
        replaceScriptToken(persistenceScript, "__OPENMW_PERSISTENT_ROOT__", toJavaScriptStringLiteral(persistentRootPath));
        replaceScriptToken(persistenceScript, "__OPENMW_HOME_PATH__", toJavaScriptStringLiteral(homePath));
        replaceScriptToken(persistenceScript, "__OPENMW_CONFIG_PATH__", toJavaScriptStringLiteral(configPath));
        replaceScriptToken(persistenceScript, "__OPENMW_DATA_PATH__", toJavaScriptStringLiteral(dataPath));
        emscripten_run_script(persistenceScript.c_str());

        setenv("HOME", homePath.c_str(), 1);
        setenv("XDG_CONFIG_HOME", configPath.c_str(), 1);
        setenv("XDG_DATA_HOME", dataPath.c_str(), 1);
    }
#endif

    class OSGLogHandler : public osg::NotifyHandler
    {
        void notify(osg::NotifySeverity severity, const char* msg) override
        {
            // Copy, because osg logging is not thread safe.
            std::string msgCopy(msg);
            if (msgCopy.empty())
                return;

            Debug::Level level;
            switch (severity)
            {
                case osg::ALWAYS:
                case osg::FATAL:
                    level = Debug::Error;
                    break;
                case osg::WARN:
                case osg::NOTICE:
                    level = Debug::Warning;
                    break;
                case osg::INFO:
                    level = Debug::Info;
                    break;
                case osg::DEBUG_INFO:
                case osg::DEBUG_FP:
                default:
                    level = Debug::Debug;
            }
            std::string_view s(msgCopy);
            if (s.size() < 1024)
                Log(level) << (s.back() == '\n' ? s.substr(0, s.size() - 1) : s);
            else
            {
                while (!s.empty())
                {
                    size_t lineSize = 1;
                    while (lineSize < s.size() && s[lineSize - 1] != '\n')
                        lineSize++;
                    Log(level) << s.substr(0, s[lineSize - 1] == '\n' ? lineSize - 1 : lineSize);
                    s = s.substr(lineSize);
                }
            }
        }
    };
}

int runApplication(int argc, char* argv[])
{
    Platform::init();

#ifdef __EMSCRIPTEN__
    initializeWasmPersistentStorage();
    bootstrapWasmConfigFile();
    OMW::WasmFilePicker::initialize("/gamedata");
    OMW::WasmFilePicker::registerBrowserCallbacks();
#endif

#ifdef __APPLE__
    setenv("OSG_GL_TEXTURE_STORAGE", "OFF", 0);
#endif

    osg::setNotifyHandler(new OSGLogHandler());
    Files::ConfigurationManager cfgMgr;
    std::unique_ptr<OMW::Engine> engine = std::make_unique<OMW::Engine>(cfgMgr);

    engine->setRecastMaxLogLevel(Debug::getRecastMaxLogLevel());

    if (parseOptions(argc, argv, *engine, cfgMgr))
    {
        if (!Misc::checkRequiredOSGPluginsArePresent())
            return 1;

        engine->go();
    }

    return 0;
}

#ifdef ANDROID
extern "C" int SDL_main(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
    return Debug::wrapApplication(&runApplication, argc, argv, "OpenMW");
}

// Platform specific for Windows when there is no console built into the executable.
// Windows will call the WinMain function instead of main in this case, the normal
// main function is then called with the __argc and __argv parameters.
#if defined(_WIN32) && !defined(_CONSOLE)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    return main(__argc, __argv);
}
#endif
