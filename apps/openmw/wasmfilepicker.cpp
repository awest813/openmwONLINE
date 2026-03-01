#ifdef __EMSCRIPTEN__

#include "wasmfilepicker.hpp"

#include <algorithm>
#include <sstream>

#include <emscripten.h>

#include <components/debug/debuglog.hpp>

namespace
{
    std::filesystem::path sDataMountPath;
    bool sDataReady = false;
    bool sEssentialFilesReady = false;
    int sTotalFileCount = 0;
    int sUploadedFileCount = 0;
}

extern "C"
{
    EMSCRIPTEN_KEEPALIVE
    void openmw_wasm_notify_data_ready()
    {
        sDataReady = true;
        Log(Debug::Info) << "WASM: Game data upload complete, data ready at "
                         << sDataMountPath.string();
    }

    EMSCRIPTEN_KEEPALIVE
    void openmw_wasm_notify_essential_ready()
    {
        sEssentialFilesReady = true;
        Log(Debug::Info) << "WASM: Essential game files loaded (ESM/ESP/BSA)";
    }

    EMSCRIPTEN_KEEPALIVE
    void openmw_wasm_set_total_file_count(int count)
    {
        sTotalFileCount = count;
    }

    EMSCRIPTEN_KEEPALIVE
    void openmw_wasm_set_uploaded_file_count(int count)
    {
        sUploadedFileCount = count;
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
}

namespace OMW::WasmFilePicker
{
    void initialize(const std::filesystem::path& dataMount)
    {
        sDataMountPath = dataMount;
        sDataReady = false;
        sEssentialFilesReady = false;
        sTotalFileCount = 0;
        sUploadedFileCount = 0;

        std::string mountStr = dataMount.string();
        std::string initScript = R"(
            (function() {
                var mountPath = )" + std::string("'") + mountStr + std::string("'") + R"(;

                if (!FS.analyzePath(mountPath).exists)
                    FS.mkdir(mountPath);

                if (typeof globalThis !== 'undefined') {
                    globalThis.__openmwDataMountPath = mountPath;

                    globalThis.__openmwUploadFile = function(relativePath, data) {
                        var fullPath = mountPath + '/' + relativePath;
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

                    // Check if a filename is an essential game file (ESM, ESP, BSA, omwaddon)
                    globalThis.__openmwIsEssentialFile = function(name) {
                        var lower = name.toLowerCase();
                        return lower.endsWith('.esm') || lower.endsWith('.esp')
                            || lower.endsWith('.bsa') || lower.endsWith('.omwaddon')
                            || lower.endsWith('.omwgame') || lower.endsWith('.omwscripts')
                            || lower === 'openmw.cfg';
                    };

                    globalThis.__openmwPickDataDirectory = async function() {
                        if (typeof window === 'undefined' || !window.showDirectoryPicker) {
                            console.error('File System Access API not available.');
                            return;
                        }

                        try {
                            var dirHandle = await window.showDirectoryPicker({ mode: 'read' });
                            console.log('Selected directory:', dirHandle.name);

                            // Phase 1: Scan directory structure and categorize files
                            var essentialFiles = [];
                            var deferredFiles = [];

                            async function scanDirectory(handle, pathPrefix) {
                                for await (var entry of handle.values()) {
                                    if (entry.kind === 'file') {
                                        var relativePath = pathPrefix + entry.name;
                                        if (globalThis.__openmwIsEssentialFile(entry.name)) {
                                            essentialFiles.push({ handle: entry, path: relativePath });
                                        } else {
                                            deferredFiles.push({ handle: entry, path: relativePath });
                                        }
                                    } else if (entry.kind === 'directory') {
                                        await scanDirectory(entry, pathPrefix + entry.name + '/');
                                    }
                                }
                            }

                            console.log('Scanning directory structure...');
                            await scanDirectory(dirHandle, '');

                            var totalFiles = essentialFiles.length + deferredFiles.length;
                            _openmw_wasm_set_total_file_count(totalFiles);
                            console.log('Found', totalFiles, 'files (' + essentialFiles.length + ' essential, ' + deferredFiles.length + ' deferred)');

                            // Phase 2: Upload essential files first
                            var uploadCount = 0;
                            console.log('Uploading essential game files...');
                            for (var i = 0; i < essentialFiles.length; i++) {
                                var entry = essentialFiles[i];
                                var file = await entry.handle.getFile();
                                var buffer = await file.arrayBuffer();
                                globalThis.__openmwUploadFile(entry.path, buffer);
                                uploadCount++;
                                _openmw_wasm_set_uploaded_file_count(uploadCount);
                                if (typeof Module !== 'undefined' && Module.setStatus)
                                    Module.setStatus('Loading essential files: ' + uploadCount + '/' + essentialFiles.length);
                            }
                            console.log('Essential files loaded:', essentialFiles.length);
                            _openmw_wasm_notify_essential_ready();

                            // Phase 3: Upload remaining files in batches to avoid blocking
                            var BATCH_SIZE = 50;
                            console.log('Loading remaining assets in background...');
                            for (var batch = 0; batch < deferredFiles.length; batch += BATCH_SIZE) {
                                var end = Math.min(batch + BATCH_SIZE, deferredFiles.length);
                                var promises = [];
                                for (var j = batch; j < end; j++) {
                                    promises.push((async function(entry) {
                                        var file = await entry.handle.getFile();
                                        var buffer = await file.arrayBuffer();
                                        globalThis.__openmwUploadFile(entry.path, buffer);
                                    })(deferredFiles[j]));
                                }
                                await Promise.all(promises);
                                uploadCount += (end - batch);
                                _openmw_wasm_set_uploaded_file_count(uploadCount);
                                if (uploadCount % 200 === 0 || batch + BATCH_SIZE >= deferredFiles.length)
                                    console.log('Uploaded', uploadCount, '/', totalFiles, 'files...');
                                // Yield to browser event loop between batches
                                await new Promise(function(r) { setTimeout(r, 0); });
                            }

                            console.log('Upload complete:', uploadCount, 'files');
                            globalThis.__openmwNotifyDataReady();
                        } catch (e) {
                            console.error('Directory picker error:', e);
                        }
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

    bool areEssentialFilesReady()
    {
        return sEssentialFilesReady;
    }

    int getTotalFileCount()
    {
        return sTotalFileCount;
    }

    int getUploadedFileCount()
    {
        return sUploadedFileCount;
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
}

#endif
