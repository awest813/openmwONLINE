#ifdef __EMSCRIPTEN__

#include "wasmfilepicker.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string_view>

#include <emscripten.h>

#include <components/debug/debuglog.hpp>

namespace
{
    std::filesystem::path sDataMountPath;
    bool sDataReady = false;
    uint32_t sUploadedFileCount = 0;
    uint64_t sUploadedByteCount = 0;
}

extern "C"
{
    EMSCRIPTEN_KEEPALIVE
    void openmw_wasm_notify_data_ready()
    {
        sDataReady = true;
        Log(Debug::Info) << "WASM: Game data upload complete - "
                         << sUploadedFileCount << " files, "
                         << (static_cast<double>(sUploadedByteCount) / (1024.0 * 1024.0)) << " MB at "
                         << sDataMountPath.string();
        OMW::WasmFilePicker::updateCfgWithExpansions();
    }

    EMSCRIPTEN_KEEPALIVE
    int openmw_wasm_is_data_ready()
    {
        return sDataReady ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE
    const char* openmw_wasm_get_data_path()
    {
        static std::string pathStr;
        pathStr = sDataMountPath.string();
        return pathStr.c_str();
    }

    EMSCRIPTEN_KEEPALIVE
    void openmw_wasm_report_upload_progress(uint32_t fileCount, double totalBytes)
    {
        sUploadedFileCount = fileCount;
        sUploadedByteCount = static_cast<uint64_t>(totalBytes);
    }
}

namespace OMW::WasmFilePicker
{
    void initialize(const std::filesystem::path& dataMount)
    {
        sDataMountPath = dataMount;
        sDataReady = false;
        sUploadedFileCount = 0;
        sUploadedByteCount = 0;

        std::string mountStr = dataMount.string();
        std::string escapedMount;
        for (char c : mountStr)
        {
            switch (c)
            {
                case '\\': escapedMount += "\\\\"; break;
                case '\'': escapedMount += "\\'"; break;
                case '\n': escapedMount += "\\n"; break;
                case '\r': escapedMount += "\\r"; break;
                case '\t': escapedMount += "\\t"; break;
                case '\0': escapedMount += "\\0"; break;
                default: escapedMount += c; break;
            }
        }

        std::string initScript = R"(
            (function() {
                var mountPath = ')" + escapedMount + R"(';

                if (!FS.analyzePath(mountPath).exists)
                    FS.mkdir(mountPath);

                if (typeof globalThis !== 'undefined') {
                    globalThis.__openmwDataMountPath = mountPath;
                    globalThis.__openmwUploadStats = { files: 0, bytes: 0, totalFiles: 0, totalBytes: 0 };
                    globalThis.__openmwLastPickResult = { status: 'idle', message: '' };

                    globalThis.__openmwSetLastPickResult = function(status, message) {
                        globalThis.__openmwLastPickResult = {
                            status: status || 'unknown',
                            message: message || '',
                        };
                    };

                    globalThis.__openmwGetLastPickResult = function() {
                        return globalThis.__openmwLastPickResult;
                    };

                    function getPickerCapabilities() {
                        var fileInputProbe = typeof document !== 'undefined'
                            && typeof document.createElement === 'function'
                            ? document.createElement('input')
                            : null;
                        var hasDirectoryPicker = typeof window !== 'undefined'
                            && typeof window.showDirectoryPicker === 'function';
                        var hasFileInputFallback = !!fileInputProbe
                            && (
                                'webkitdirectory' in fileInputProbe
                                || 'mozdirectory' in fileInputProbe
                                || 'directory' in fileInputProbe
                            );
                        return {
                            directoryPicker: hasDirectoryPicker,
                            fileInputFallback: hasFileInputFallback,
                            supported: hasDirectoryPicker || hasFileInputFallback,
                        };
                    }

                    globalThis.__openmwGetPickerCapabilities = getPickerCapabilities;

                    function normalizeRelativePath(path) {
                        if (!path)
                            return '';
                        var sanitized = String(path).replace(/\\/g, '/');
                        var parts = sanitized.split('/');
                        var safeParts = [];
                        for (var i = 0; i < parts.length; i++) {
                            var part = parts[i];
                            if (!part || part === '.' || part === '..')
                                continue;
                            safeParts.push(part);
                        }
                        return safeParts.join('/');
                    }

                    function createFileEntry(path, size, openFileFn) {
                        var normalized = normalizeRelativePath(path);
                        if (!normalized)
                            return null;
                        return {
                            path: normalized,
                            size: size,
                            openFile: openFileFn,
                        };
                    }

                    function removeFsPathRecursive(path) {
                        var analysis = FS.analyzePath(path);
                        if (!analysis.exists)
                            return;

                        var stat = FS.stat(path);
                        if (FS.isDir(stat.mode)) {
                            var children = FS.readdir(path);
                            for (var i = 0; i < children.length; i++) {
                                var child = children[i];
                                if (child === '.' || child === '..')
                                    continue;
                                removeFsPathRecursive(path + '/' + child);
                            }
                            FS.rmdir(path);
                        } else {
                            FS.unlink(path);
                        }
                    }

                    function clearDataMountDirectory() {
                        if (!FS.analyzePath(mountPath).exists) {
                            FS.mkdir(mountPath);
                            return;
                        }

                        var children = FS.readdir(mountPath);
                        for (var i = 0; i < children.length; i++) {
                            var child = children[i];
                            if (child === '.' || child === '..')
                                continue;
                            removeFsPathRecursive(mountPath + '/' + child);
                        }
                    }

                    globalThis.__openmwUploadFile = function(relativePath, data) {
                        var normalizedPath = normalizeRelativePath(relativePath);
                        if (!normalizedPath)
                            return;
                        var fullPath = mountPath + '/' + normalizedPath;
                        var parts = fullPath.split('/');
                        var current = '';
                        for (var i = 1; i < parts.length - 1; i++) {
                            current += '/' + parts[i];
                            if (!FS.analyzePath(current).exists)
                                FS.mkdir(current);
                        }
                        FS.writeFile(fullPath, new Uint8Array(data));
                    };

                    globalThis.__openmwNotifyDataReady = function() {
                        _openmw_wasm_notify_data_ready();
                    };

                    globalThis.__openmwReportProgress = function(fileCount, totalBytes) {
                        _openmw_wasm_report_upload_progress(fileCount, totalBytes);
                    };

                    async function enumerateFiles(handle, pathPrefix) {
                        var entries = [];
                        try {
                            for await (const entry of handle.values()) {
                                if (entry.kind === 'file') {
                                    try {
                                        var file = await entry.getFile();
                                        var fileEntry = createFileEntry(pathPrefix + entry.name, file.size, function() {
                                            return entry.getFile();
                                        });
                                        if (fileEntry)
                                            entries.push(fileEntry);
                                    } catch (fileErr) {
                                        console.warn('Skipping unreadable file:', pathPrefix + entry.name, fileErr.message);
                                    }
                                } else if (entry.kind === 'directory') {
                                    try {
                                        var subEntries = await enumerateFiles(entry, pathPrefix + entry.name + '/');
                                        entries = entries.concat(subEntries);
                                    } catch (dirErr) {
                                        console.warn('Skipping inaccessible directory:', pathPrefix + entry.name, dirErr.message);
                                    }
                                }
                            }
                        } catch (iterErr) {
                            console.warn('Failed to iterate directory:', pathPrefix, iterErr.message);
                        }
                        return entries;
                    }

                    async function pickFilesViaInput() {
                        if (typeof document === 'undefined'
                            || typeof document.createElement !== 'function'
                            || !document.body) {
                            return null;
                        }

                        return await new Promise(function(resolve, reject) {
                            var input = document.createElement('input');
                            input.type = 'file';
                            input.multiple = true;
                            input.setAttribute('webkitdirectory', '');
                            input.setAttribute('mozdirectory', '');
                            input.setAttribute('directory', '');
                            input.style.position = 'fixed';
                            input.style.left = '-9999px';
                            input.style.top = '-9999px';

                            var settled = false;
                            var focusTimerId = null;

                            function cleanup() {
                                if (focusTimerId !== null)
                                    clearTimeout(focusTimerId);
                                if (typeof window !== 'undefined')
                                    window.removeEventListener('focus', onWindowFocus);
                                if (input.parentNode)
                                    input.parentNode.removeChild(input);
                            }

                            function resolveOnce(value) {
                                if (settled)
                                    return;
                                settled = true;
                                cleanup();
                                resolve(value);
                            }

                            function rejectOnce(error) {
                                if (settled)
                                    return;
                                settled = true;
                                cleanup();
                                reject(error);
                            }

                            function onWindowFocus() {
                                if (focusTimerId !== null)
                                    clearTimeout(focusTimerId);
                                focusTimerId = setTimeout(function() {
                                    if (!settled)
                                        resolveOnce(null);
                                }, 0);
                            }

                            input.addEventListener('change', function() {
                                try {
                                    var selectedFiles = Array.prototype.slice.call(input.files || []);
                                    if (!selectedFiles.length) {
                                        resolveOnce(null);
                                        return;
                                    }

                                    var entries = selectedFiles.map(function(file) {
                                        var relativePath = file.webkitRelativePath || file.relativePath || file.name;
                                        return createFileEntry(relativePath, file.size, function() {
                                            return Promise.resolve(file);
                                        });
                                    }).filter(function(entry) { return !!entry; });

                                    resolveOnce(entries.length ? entries : null);
                                } catch (error) {
                                    rejectOnce(error);
                                }
                            });

                            if (typeof window !== 'undefined')
                                window.addEventListener('focus', onWindowFocus);

                            document.body.appendChild(input);
                            try {
                                input.click();
                            } catch (error) {
                                rejectOnce(error);
                            }
                        });
                    }

                    var CHUNK_SIZE = 8 * 1024 * 1024;
                    var REQUIRED_BASE_FILES = ['morrowind.esm'];

                    async function uploadFileChunked(file, relativePath) {
                        var normalizedPath = normalizeRelativePath(relativePath);
                        if (!normalizedPath)
                            return;
                        if (file.size <= CHUNK_SIZE) {
                            var buffer = await file.arrayBuffer();
                            globalThis.__openmwUploadFile(normalizedPath, buffer);
                        } else {
                            var fullPath = mountPath + '/' + normalizedPath;
                            var parts = fullPath.split('/');
                            var current = '';
                            for (var i = 1; i < parts.length - 1; i++) {
                                current += '/' + parts[i];
                                if (!FS.analyzePath(current).exists)
                                    FS.mkdir(current);
                            }
                            var stream = FS.open(fullPath, 'w');
                            try {
                                var offset = 0;
                                while (offset < file.size) {
                                    var end = Math.min(offset + CHUNK_SIZE, file.size);
                                    var slice = file.slice(offset, end);
                                    var chunk = new Uint8Array(await slice.arrayBuffer());
                                    FS.write(stream, chunk, 0, chunk.length);
                                    offset = end;
                                }
                            } finally {
                                FS.close(stream);
                            }
                        }
                    }

                    function hasRequiredBaseFiles(fileEntries) {
                        var normalizedPaths = fileEntries.map(function(entry) {
                            return entry.path.toLowerCase();
                        });

                        return REQUIRED_BASE_FILES.every(function(requiredFile) {
                            return normalizedPaths.some(function(entryPath) {
                                return entryPath === requiredFile || entryPath.endsWith('/' + requiredFile);
                            });
                        });
                    }

                    globalThis.__openmwPickDataDirectory = async function() {
                        var stats = globalThis.__openmwUploadStats;
                        stats.totalFiles = 0;
                        stats.totalBytes = 0;
                        stats.files = 0;
                        stats.bytes = 0;
                        globalThis.__openmwReportProgress(0, 0);

                        var capabilities = getPickerCapabilities();
                        if (!capabilities.supported) {
                            var unsupportedMessage =
                                'This browser does not support directory import. '
                                + 'Try Chrome/Edge (directory picker) or Firefox with directory-upload support.';
                            console.error(unsupportedMessage);
                            globalThis.__openmwSetLastPickResult('unsupported', unsupportedMessage);
                            return false;
                        }

                        var fileList = null;
                        globalThis.__openmwSetLastPickResult('in-progress', '');

                        if (typeof globalThis.__openmwOnUploadPhase === 'function')
                            globalThis.__openmwOnUploadPhase('scanning');

                        if (capabilities.directoryPicker) {
                            var dirHandle;
                            try {
                                dirHandle = await window.showDirectoryPicker({ mode: 'read' });
                            } catch (e) {
                                if (e.name === 'AbortError') {
                                    console.log('Directory picker cancelled by user');
                                    globalThis.__openmwSetLastPickResult('cancelled', 'Directory selection was cancelled.');
                                    return false;
                                }
                                console.error('Directory picker error:', e);
                                throw e;
                            }

                            console.log('Selected directory:', dirHandle.name);
                            fileList = await enumerateFiles(dirHandle, '');
                        } else {
                            console.log('showDirectoryPicker unavailable; using directory-upload fallback input.');
                            try {
                                fileList = await pickFilesViaInput();
                            } catch (e) {
                                console.error('Directory upload fallback failed:', e);
                                throw e;
                            }

                            if (!fileList) {
                                console.log('Directory upload fallback cancelled by user');
                                globalThis.__openmwSetLastPickResult('cancelled', 'Directory selection was cancelled.');
                                return false;
                            }
                        }

                        if (!fileList || !fileList.length) {
                            var emptySelectionMessage = 'No files were selected. Please choose the Morrowind Data Files folder.';
                            console.warn(emptySelectionMessage);
                            globalThis.__openmwSetLastPickResult('empty-selection', emptySelectionMessage);
                            return false;
                        }

                        if (!hasRequiredBaseFiles(fileList)) {
                            var validationMessage =
                                'Selected folder is missing required base content (Morrowind.esm). '
                                + 'Please choose the Morrowind Data Files directory.';
                            console.warn(validationMessage);
                            globalThis.__openmwSetLastPickResult('validation-failed', validationMessage);
                            return false;
                        }

                        var totalBytes = fileList.reduce(function(sum, f) { return sum + f.size; }, 0);
                        stats.totalFiles = fileList.length;
                        stats.totalBytes = totalBytes;
                        stats.files = 0;
                        stats.bytes = 0;

                        console.log('Found', fileList.length, 'files (' + (totalBytes / (1024*1024)).toFixed(1) + ' MB)');

                        try {
                            clearDataMountDirectory();
                        } catch (clearError) {
                            console.error('Failed to clear existing uploaded data:', clearError);
                            throw new Error(
                                'Failed to reset previous uploaded data: '
                                + (clearError && clearError.message ? clearError.message : String(clearError))
                            );
                        }

                        if (typeof globalThis.__openmwOnUploadPhase === 'function')
                            globalThis.__openmwOnUploadPhase('uploading');

                        for (var i = 0; i < fileList.length; i++) {
                            var uploadedFile = await fileList[i].openFile();
                            await uploadFileChunked(uploadedFile, fileList[i].path);
                            stats.files++;
                            stats.bytes += fileList[i].size;

                            if (typeof globalThis.__openmwOnUploadProgress === 'function')
                                globalThis.__openmwOnUploadProgress(stats.files, stats.totalFiles, stats.bytes, stats.totalBytes);

                            globalThis.__openmwReportProgress(stats.files, stats.bytes);

                            if ((i + 1) % 50 === 0)
                                await new Promise(function(r) { setTimeout(r, 0); });
                        }

                        console.log('Upload complete:', stats.files, 'files,', (stats.bytes / (1024*1024)).toFixed(1), 'MB');
                        globalThis.__openmwSetLastPickResult('success', 'Game data loaded successfully.');
                        globalThis.__openmwNotifyDataReady();
                        return true;
                    };
                }

                console.log('OpenMW WASM file picker initialized. Mount path:', mountPath);
                console.log('Call __openmwPickDataDirectory() to select your Morrowind data folder.');
            })();
        )";

        emscripten_run_script(initScript.c_str());

        Log(Debug::Info) << "WASM: File picker initialized, data mount at " << mountStr;
    }

    bool isDataReady()
    {
        return sDataReady;
    }

    const std::filesystem::path& getDataPath()
    {
        return sDataMountPath;
    }

    std::vector<std::string> listUploadedFiles()
    {
        std::vector<std::string> files;

        if (!std::filesystem::exists(sDataMountPath))
            return files;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(sDataMountPath))
        {
            if (!entry.is_directory())
            {
                auto relative = std::filesystem::relative(entry.path(), sDataMountPath);
                files.push_back(relative.string());
            }
        }

        std::sort(files.begin(), files.end());
        return files;
    }

    void registerBrowserCallbacks()
    {
        emscripten_run_script(R"(
            if (typeof globalThis !== 'undefined') {
                globalThis.__openmwIsDataReady = function() {
                    return _openmw_wasm_is_data_ready() !== 0;
                };
                globalThis.__openmwGetDataPath = function() {
                    return UTF8ToString(_openmw_wasm_get_data_path());
                };
            }
        )");
    }

    uint32_t getUploadedFileCount()
    {
        return sUploadedFileCount;
    }

    uint64_t getUploadedByteCount()
    {
        return sUploadedByteCount;
    }

    void updateCfgWithExpansions()
    {
        // Look for Tribunal and Bloodmoon ESMs in the uploaded data.
        // Files are expected directly in the data mount (i.e. the user selected the
        // "Data Files" folder, so ESMs sit at the root of /gamedata).
        const auto checkExpansion = [](const std::filesystem::path& base, const char* esm) -> bool {
            // Try the canonical capitalisation first, then fully lower-case.
            if (std::filesystem::exists(base / esm))
                return true;
            std::string lower(esm);
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return std::filesystem::exists(base / lower);
        };

        const bool hasTribunal = checkExpansion(sDataMountPath, "Tribunal.esm");
        const bool hasBloodmoon = checkExpansion(sDataMountPath, "Bloodmoon.esm");

        if (!hasTribunal && !hasBloodmoon)
        {
            Log(Debug::Info) << "WASM: No expansion ESMs detected in uploaded data";
            return;
        }

        // Locate the config file written by bootstrapWasmConfigFile().
        const char* cfgHome = std::getenv("XDG_CONFIG_HOME");
        const std::string configBase = (cfgHome && cfgHome[0] != '\0') ? cfgHome : "/persistent/home/.config";
        const std::string cfgFile = configBase + "/openmw/openmw.cfg";

        if (!std::filesystem::exists(cfgFile))
        {
            Log(Debug::Warning) << "WASM: Config file not found; cannot update for detected expansions";
            return;
        }

        std::ifstream in(cfgFile);
        if (!in.is_open())
        {
            Log(Debug::Warning) << "WASM: Cannot open config file for reading";
            return;
        }

        std::ostringstream buf;
        buf << in.rdbuf();
        in.close();
        std::string content = buf.str();

        // Uncomment entries for detected expansions.  The commented lines were
        // written by bootstrapWasmConfigFile() with exactly these tokens.
        // Each token appears at most once in the auto-generated config, so a
        // single find-and-replace per entry is sufficient.
        const auto uncomment = [&content](std::string_view commentedLine, std::string_view activeLine) {
            auto pos = content.find(commentedLine);
            if (pos != std::string::npos)
                content.replace(pos, commentedLine.size(), activeLine);
        };

        if (hasTribunal)
        {
            uncomment("# content=Tribunal.esm", "content=Tribunal.esm");
            uncomment("# fallback-archive=Tribunal.bsa", "fallback-archive=Tribunal.bsa");
            Log(Debug::Info) << "WASM: Enabling Tribunal expansion in config";
        }

        if (hasBloodmoon)
        {
            uncomment("# content=Bloodmoon.esm", "content=Bloodmoon.esm");
            uncomment("# fallback-archive=Bloodmoon.bsa", "fallback-archive=Bloodmoon.bsa");
            Log(Debug::Info) << "WASM: Enabling Bloodmoon expansion in config";
        }

        std::ofstream out(cfgFile);
        if (!out.is_open())
        {
            Log(Debug::Warning) << "WASM: Cannot write updated config file";
            return;
        }

        out << content;
        out.close();

        Log(Debug::Info) << "WASM: Config updated with detected expansions (takes effect on next page reload)";

        // Trigger an IDBFS flush so the updated config reaches persistent storage
        // before the user reloads the page.  Any flush error is logged by the
        // existing JavaScript callback registered in initializeWasmPersistentStorage().
        // The availability check and invocation are kept in a single script call to
        // avoid a race between the two emscripten_run_script calls.
        emscripten_run_script(R"(
            if (typeof globalThis !== 'undefined'
                    && typeof globalThis.__openmwSyncPersistentStorage === 'function') {
                globalThis.__openmwSyncPersistentStorage();
            } else {
                console.warn('OpenMW WASM: persistent storage sync unavailable; '
                    + 'expansion config changes will be saved on the next periodic sync.');
            }
        )");
    }
}

#endif
