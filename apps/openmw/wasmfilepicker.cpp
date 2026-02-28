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

                    globalThis.__openmwPickDataDirectory = async function() {
                        if (typeof window === 'undefined' || !window.showDirectoryPicker) {
                            console.error('File System Access API not available.');
                            return;
                        }

                        try {
                            var dirHandle = await window.showDirectoryPicker({ mode: 'read' });
                            console.log('Selected directory:', dirHandle.name);

                            var uploadCount = 0;
                            async function processDirectory(handle, pathPrefix) {
                                for await (var entry of handle.values()) {
                                    if (entry.kind === 'file') {
                                        var file = await entry.getFile();
                                        var buffer = await file.arrayBuffer();
                                        var relativePath = pathPrefix + entry.name;
                                        globalThis.__openmwUploadFile(relativePath, buffer);
                                        uploadCount++;
                                        if (uploadCount % 100 === 0)
                                            console.log('Uploaded', uploadCount, 'files...');
                                    } else if (entry.kind === 'directory') {
                                        await processDirectory(entry, pathPrefix + entry.name + '/');
                                    }
                                }
                            }

                            await processDirectory(dirHandle, '');
                            console.log('Upload complete:', uploadCount, 'files');
                            globalThis.__openmwNotifyDataReady();
                        } catch (e) {
                            console.error('Directory picker error:', e);
                        }
                    };
                }

                    globalThis.__openmwCacheToOPFS = async function() {
                        if (!navigator.storage || !navigator.storage.getDirectory) {
                            console.warn('OPFS not available in this browser');
                            return;
                        }
                        try {
                            var opfsRoot = await navigator.storage.getDirectory();
                            var openmwDir = await opfsRoot.getDirectoryHandle('openmw-data', { create: true });

                            async function cacheDir(emPath, opfsParent) {
                                var entries = FS.readdir(emPath).filter(function(e) { return e !== '.' && e !== '..'; });
                                for (var i = 0; i < entries.length; i++) {
                                    var fullPath = emPath + '/' + entries[i];
                                    var stat = FS.stat(fullPath);
                                    if (FS.isDir(stat.mode)) {
                                        var subDir = await opfsParent.getDirectoryHandle(entries[i], { create: true });
                                        await cacheDir(fullPath, subDir);
                                    } else {
                                        var fileHandle = await opfsParent.getFileHandle(entries[i], { create: true });
                                        var writable = await fileHandle.createWritable();
                                        var data = FS.readFile(fullPath);
                                        await writable.write(data);
                                        await writable.close();
                                    }
                                }
                            }

                            console.log('Caching game data to OPFS...');
                            await cacheDir(mountPath, openmwDir);
                            console.log('OPFS cache complete');
                        } catch (e) {
                            console.error('OPFS cache error:', e);
                        }
                    };

                    globalThis.__openmwRestoreFromOPFS = async function() {
                        if (!navigator.storage || !navigator.storage.getDirectory)
                            return false;
                        try {
                            var opfsRoot = await navigator.storage.getDirectory();
                            var openmwDir = await opfsRoot.getDirectoryHandle('openmw-data');
                            var count = 0;

                            async function restoreDir(opfsParent, emPath) {
                                for await (var [name, handle] of opfsParent) {
                                    var fullPath = emPath + '/' + name;
                                    if (handle.kind === 'directory') {
                                        if (!FS.analyzePath(fullPath).exists) FS.mkdir(fullPath);
                                        await restoreDir(handle, fullPath);
                                    } else {
                                        var file = await handle.getFile();
                                        var buf = await file.arrayBuffer();
                                        var parts = fullPath.split('/');
                                        var dir = '';
                                        for (var i = 1; i < parts.length - 1; i++) {
                                            dir += '/' + parts[i];
                                            if (!FS.analyzePath(dir).exists) FS.mkdir(dir);
                                        }
                                        FS.writeFile(fullPath, new Uint8Array(buf));
                                        count++;
                                    }
                                }
                            }

                            await restoreDir(openmwDir, mountPath);
                            if (count > 0) {
                                console.log('Restored', count, 'files from OPFS cache');
                                _openmw_wasm_notify_data_ready();
                                return true;
                            }
                            return false;
                        } catch (e) {
                            return false;
                        }
                    };

                console.log('OpenMW WASM file picker initialized. Mount path:', mountPath);
                console.log('Call __openmwPickDataDirectory() to select your Morrowind data folder.');
                console.log('Call __openmwRestoreFromOPFS() to restore cached data.');
                console.log('Call __openmwCacheToOPFS() after loading to cache data for next visit.');
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
}

#endif
