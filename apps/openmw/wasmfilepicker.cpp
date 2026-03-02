#ifdef __EMSCRIPTEN__

#include "wasmfilepicker.hpp"

#include <algorithm>

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
                         << (sUploadedByteCount / (1024 * 1024)) << " MB at "
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

    EMSCRIPTEN_KEEPALIVE
    void openmw_wasm_report_upload_progress(uint32_t fileCount, uint32_t totalBytes)
    {
        sUploadedFileCount = fileCount;
        sUploadedByteCount = totalBytes;
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
            if (c == '\\' || c == '\'')
                escapedMount += '\\';
            escapedMount += c;
        }

        std::string initScript = R"(
            (function() {
                var mountPath = ')" + escapedMount + R"(';

                if (!FS.analyzePath(mountPath).exists)
                    FS.mkdir(mountPath);

                if (typeof globalThis !== 'undefined') {
                    globalThis.__openmwDataMountPath = mountPath;
                    globalThis.__openmwUploadStats = { files: 0, bytes: 0, totalFiles: 0, totalBytes: 0 };

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

                    globalThis.__openmwReportProgress = function(fileCount, totalBytes) {
                        _openmw_wasm_report_upload_progress(fileCount, totalBytes);
                    };

                    async function enumerateFiles(handle, pathPrefix) {
                        var entries = [];
                        for await (var entry of handle.values()) {
                            if (entry.kind === 'file') {
                                var file = await entry.getFile();
                                entries.push({ path: pathPrefix + entry.name, handle: entry, size: file.size });
                            } else if (entry.kind === 'directory') {
                                var subEntries = await enumerateFiles(entry, pathPrefix + entry.name + '/');
                                entries = entries.concat(subEntries);
                            }
                        }
                        return entries;
                    }

                    var CHUNK_SIZE = 8 * 1024 * 1024;

                    async function uploadFileChunked(entry, relativePath) {
                        var file = await entry.handle.getFile();
                        if (file.size <= CHUNK_SIZE) {
                            var buffer = await file.arrayBuffer();
                            globalThis.__openmwUploadFile(relativePath, buffer);
                        } else {
                            var fullPath = mountPath + '/' + relativePath;
                            var parts = fullPath.split('/');
                            var current = '';
                            for (var i = 1; i < parts.length - 1; i++) {
                                current += '/' + parts[i];
                                if (!FS.analyzePath(current).exists)
                                    FS.mkdir(current);
                            }
                            var stream = FS.open(fullPath, 'w');
                            var offset = 0;
                            while (offset < file.size) {
                                var end = Math.min(offset + CHUNK_SIZE, file.size);
                                var slice = file.slice(offset, end);
                                var chunk = new Uint8Array(await slice.arrayBuffer());
                                FS.write(stream, chunk, 0, chunk.length);
                                offset = end;
                            }
                            FS.close(stream);
                        }
                    }

                    globalThis.__openmwPickDataDirectory = async function() {
                        if (typeof window === 'undefined' || !window.showDirectoryPicker) {
                            console.error('File System Access API not available.');
                            return false;
                        }

                        var dirHandle;
                        try {
                            dirHandle = await window.showDirectoryPicker({ mode: 'read' });
                        } catch (e) {
                            if (e.name === 'AbortError') {
                                console.log('Directory picker cancelled by user');
                                return false;
                            }
                            console.error('Directory picker error:', e);
                            throw e;
                        }

                        console.log('Selected directory:', dirHandle.name);

                        if (typeof globalThis.__openmwOnUploadPhase === 'function')
                            globalThis.__openmwOnUploadPhase('scanning');

                        var fileList = await enumerateFiles(dirHandle, '');
                        var totalBytes = fileList.reduce(function(sum, f) { return sum + f.size; }, 0);
                        var stats = globalThis.__openmwUploadStats;
                        stats.totalFiles = fileList.length;
                        stats.totalBytes = totalBytes;
                        stats.files = 0;
                        stats.bytes = 0;

                        console.log('Found', fileList.length, 'files (' + (totalBytes / (1024*1024)).toFixed(1) + ' MB)');

                        if (typeof globalThis.__openmwOnUploadPhase === 'function')
                            globalThis.__openmwOnUploadPhase('uploading');

                        for (var i = 0; i < fileList.length; i++) {
                            await uploadFileChunked(fileList[i], fileList[i].path);
                            stats.files++;
                            stats.bytes += fileList[i].size;

                            if (typeof globalThis.__openmwOnUploadProgress === 'function')
                                globalThis.__openmwOnUploadProgress(stats.files, stats.totalFiles, stats.bytes, stats.totalBytes);

                            globalThis.__openmwReportProgress(stats.files, stats.bytes);

                            if (i % 50 === 0)
                                await new Promise(function(r) { setTimeout(r, 0); });
                        }

                        console.log('Upload complete:', stats.files, 'files,', (stats.bytes / (1024*1024)).toFixed(1), 'MB');
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
}

#endif
