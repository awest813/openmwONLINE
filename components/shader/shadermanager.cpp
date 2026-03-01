#include "shadermanager.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <system_error>
#include <unordered_map>

#include <osg/Program>
#include <osgViewer/Viewer>

#include <components/debug/debuglog.hpp>
#include <components/files/conversion.hpp>
#include <components/misc/pathhelpers.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/conversion.hpp>
#include <components/settings/settings.hpp>

namespace
{
    osg::Shader::Type getShaderType(const std::string& templateName)
    {
        std::string_view ext = Misc::getFileExtension(templateName);

        if (ext == "vert")
            return osg::Shader::VERTEX;
        if (ext == "frag")
            return osg::Shader::FRAGMENT;
        if (ext == "geom")
            return osg::Shader::GEOMETRY;
        if (ext == "comp")
            return osg::Shader::COMPUTE;
        if (ext == "tese")
            return osg::Shader::TESSEVALUATION;
        if (ext == "tesc")
            return osg::Shader::TESSCONTROL;

        throw std::runtime_error("unrecognized shader template name: " + templateName);
    }

    std::string_view getRootPrefix(std::string_view path)
    {
        if (path.starts_with("lib"))
            return "lib";
        else if (path.starts_with("compatibility"))
            return "compatibility";
        else if (path.starts_with("core"))
            return "core";
        return {};
    }

    int getLineNumber(std::string_view source, std::size_t foundPos, int lineNumber, int offset)
    {
        constexpr std::string_view tag = "#line";
        std::size_t lineDirectivePosition = source.rfind(tag, foundPos);
        if (lineDirectivePosition != std::string_view::npos)
        {
            std::size_t lineNumberStart = lineDirectivePosition + tag.size() + 1;
            std::size_t lineNumberEnd = source.find_first_not_of("0123456789", lineNumberStart);
            std::string_view lineNumberString = source.substr(lineNumberStart, lineNumberEnd - lineNumberStart);
            lineNumber = Misc::StringUtils::toNumeric<int>(lineNumberString, 2) + offset;
        }
        else
        {
            lineDirectivePosition = 0;
        }
        lineNumber
            += static_cast<int>(std::count(source.begin() + lineDirectivePosition, source.begin() + foundPos, '\n'));
        return lineNumber;
    }

    bool addLineDirectivesAfterConditionalBlocks(std::string& source)
    {
        for (size_t position = 0; position < source.length();)
        {
            size_t foundPos = source.find("#endif", position);
            foundPos = std::min(foundPos, source.find("#elif", position));
            foundPos = std::min(foundPos, source.find("#else", position));

            if (foundPos == std::string::npos)
                break;

            foundPos = source.find_first_of("\n\r", foundPos);
            foundPos = source.find_first_not_of("\n\r", foundPos);

            if (foundPos == std::string::npos)
                break;

            int lineNumber = getLineNumber(source, foundPos, 1, -1);

            source.replace(foundPos, 0, std::format("#line {}\n", lineNumber));

            position = foundPos;
        }

        return true;
    }

    // Recursively replaces include statements with the actual source of the included files.
    // Adjusts #line statements accordingly and detects cyclic includes.
    // cycleIncludeChecker is the set of files that include this file directly or indirectly, and is intentionally not a
    // reference to allow automatic cleanup.
    bool parseIncludes(const std::filesystem::path& shaderPath, std::string& source, const std::string& fileName,
        int& fileNumber, std::set<std::filesystem::path> cycleIncludeChecker,
        std::set<std::filesystem::path>& includedFiles)
    {
        includedFiles.insert(shaderPath / fileName);
        // An include is cyclic if it is being included by itself
        if (cycleIncludeChecker.insert(shaderPath / fileName).second == false)
        {
            Log(Debug::Error) << "Shader " << fileName << " error: Detected cyclic #includes";
            return false;
        }

        Misc::StringUtils::replaceAll(source, "\r\n", "\n");

        size_t foundPos = 0;
        while ((foundPos = source.find("#include")) != std::string::npos)
        {
            size_t start = source.find('"', foundPos);
            if (start == std::string::npos || start == source.size() - 1)
            {
                Log(Debug::Error) << "Shader " << fileName << " error: Invalid #include";
                return false;
            }
            size_t end = source.find('"', start + 1);
            if (end == std::string::npos)
            {
                Log(Debug::Error) << "Shader " << fileName << " error: Invalid #include";
                return false;
            }
            std::string includeFilename = source.substr(start + 1, end - (start + 1));

            // Check if this include is a relative path
            // TODO: We shouldn't be relying on soft-coded root prefixes, just check if the path exists and fallback to
            // searching root if it doesn't
            if (getRootPrefix(includeFilename).empty())
                includeFilename
                    = Files::pathToUnicodeString(std::filesystem::path(fileName).parent_path() / includeFilename);

            std::filesystem::path includePath = shaderPath / includeFilename;

            // Determine the line number that will be used for the #line directive following the included source
            int lineNumber = getLineNumber(source, foundPos, 0, -1);

            // Include the file recursively
            std::ifstream includeFstream;
            includeFstream.open(includePath);
            if (includeFstream.fail())
            {
                Log(Debug::Error) << "Shader " << fileName << " error: Failed to open include " << includePath << ": "
                                  << std::generic_category().message(errno);
                return false;
            }
            int includedFileNumber = fileNumber++;

            std::stringstream buffer;
            buffer << includeFstream.rdbuf();
            std::string stringRepresentation = buffer.str();
            if (!addLineDirectivesAfterConditionalBlocks(stringRepresentation)
                || !parseIncludes(
                    shaderPath, stringRepresentation, includeFilename, fileNumber, cycleIncludeChecker, includedFiles))
            {
                Log(Debug::Error) << "In file included from " << fileName << "." << lineNumber;
                return false;
            }

            std::stringstream toInsert;
            toInsert << "#line 0 " << includedFileNumber << "\n"
                     << stringRepresentation << "\n#line " << lineNumber << " 0\n";

            source.replace(foundPos, (end - foundPos + 1), toInsert.str());
        }
        return true;
    }
}

namespace
{
#ifdef __EMSCRIPTEN__
    // Preamble prepended to vertex shaders for GLES3/WebGL 2.0 compatibility.
    // Provides declarations for OSG GLES3 vertex attributes and matrix uniforms,
    // plus custom uniforms replacing deprecated fixed-function state.
    const char* const sGLES3VertexPreamble = R"(
// OSG GLES3 vertex attributes
in vec4 osg_Vertex;
in vec3 osg_Normal;
in vec4 osg_Color;
in vec4 osg_MultiTexCoord0;
in vec4 osg_MultiTexCoord1;
in vec4 osg_MultiTexCoord2;
in vec4 osg_MultiTexCoord3;
in vec4 osg_MultiTexCoord4;
in vec4 osg_MultiTexCoord5;
in vec4 osg_MultiTexCoord6;
in vec4 osg_MultiTexCoord7;

// OSG GLES3 matrix uniforms
uniform mat4 osg_ModelViewMatrix;
uniform mat4 osg_ProjectionMatrix;
uniform mat4 osg_ModelViewProjectionMatrix;
uniform mat3 osg_NormalMatrix;

// Custom uniforms replacing gl_TextureMatrix (fixed-function state)
uniform mat4 omw_TextureMatrix0;
uniform mat4 omw_TextureMatrix1;
uniform mat4 omw_TextureMatrix2;
uniform mat4 omw_TextureMatrix3;
uniform mat4 omw_TextureMatrix4;
uniform mat4 omw_TextureMatrix5;
uniform mat4 omw_TextureMatrix6;
uniform mat4 omw_TextureMatrix7;

// Custom struct replacing gl_FrontMaterial (fixed-function state)
struct OMW_MaterialParameters {
    vec4 emission;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
};
uniform OMW_MaterialParameters omw_FrontMaterial;

// Custom struct replacing gl_Fog (fixed-function state)
struct OMW_FogParameters {
    vec4 color;
    float density;
    float start;
    float end;
    float scale; // 1.0 / (end - start)
};
uniform OMW_FogParameters omw_Fog;

)";

    // Preamble prepended to fragment shaders for GLES3/WebGL 2.0 compatibility.
    const char* const sGLES3FragmentPreamble = R"(
// MRT output declarations replacing gl_FragData[]
layout(location = 0) out vec4 omw_FragData0;
layout(location = 1) out vec4 omw_FragData1;
layout(location = 2) out vec4 omw_FragData2;
layout(location = 3) out vec4 omw_FragData3;

// Custom struct replacing gl_FrontMaterial (fixed-function state)
struct OMW_MaterialParameters {
    vec4 emission;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
};
uniform OMW_MaterialParameters omw_FrontMaterial;

// Custom struct replacing gl_Fog (fixed-function state)
struct OMW_FogParameters {
    vec4 color;
    float density;
    float start;
    float end;
    float scale; // 1.0 / (end - start)
};
uniform OMW_FogParameters omw_Fog;

// Custom uniforms replacing gl_TextureMatrix (fragment shaders may also reference these)
uniform mat4 omw_TextureMatrix0;
uniform mat4 omw_TextureMatrix1;
uniform mat4 omw_TextureMatrix2;
uniform mat4 omw_TextureMatrix3;
uniform mat4 omw_TextureMatrix4;
uniform mat4 omw_TextureMatrix5;
uniform mat4 omw_TextureMatrix6;
uniform mat4 omw_TextureMatrix7;

)";

    // Replace all occurrences of 'from' with 'to' in 'source', ensuring whole-word matching
    // by checking boundaries (not alphanumeric or underscore) at both ends.
    void replaceAllWholeWord(std::string& source, const std::string& from, const std::string& to)
    {
        if (from.empty())
            return;
        size_t pos = 0;
        while ((pos = source.find(from, pos)) != std::string::npos)
        {
            // Check left boundary
            if (pos > 0)
            {
                char prev = source[pos - 1];
                if (std::isalnum(static_cast<unsigned char>(prev)) || prev == '_')
                {
                    pos += from.size();
                    continue;
                }
            }
            // Check right boundary
            size_t endPos = pos + from.size();
            if (endPos < source.size())
            {
                char next = source[endPos];
                if (std::isalnum(static_cast<unsigned char>(next)) || next == '_')
                {
                    pos += from.size();
                    continue;
                }
            }
            source.replace(pos, from.size(), to);
            pos += to.size();
        }
    }

    void replaceAllSimple(std::string& source, const std::string& from, const std::string& to)
    {
        if (from.empty())
            return;
        size_t pos = 0;
        while ((pos = source.find(from, pos)) != std::string::npos)
        {
            source.replace(pos, from.size(), to);
            pos += to.size();
        }
    }

    // Remove lines containing 'gl_ClipVertex' assignments (no ES equivalent)
    void removeClipVertexLines(std::string& source)
    {
        size_t pos = 0;
        while ((pos = source.find("gl_ClipVertex", pos)) != std::string::npos)
        {
            // Find the start of this line
            size_t lineStart = source.rfind('\n', pos);
            lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;

            // Find the end of this line
            size_t lineEnd = source.find('\n', pos);
            if (lineEnd == std::string::npos)
                lineEnd = source.size();
            else
                lineEnd += 1; // include the newline

            source.replace(lineStart, lineEnd - lineStart, "// gl_ClipVertex removed (unavailable in GLES3)\n");
            pos = lineStart + 1;
        }
    }

    // Transform shader source from GLSL 1.20 compatibility profile to GLSL ES 3.00
    // for WebGL 2.0 compatibility under Emscripten.
    void transformShaderToGLES3(std::string& source, osg::Shader::Type shaderType)
    {
        const bool isVertex = (shaderType == osg::Shader::VERTEX);
        const bool isFragment = (shaderType == osg::Shader::FRAGMENT);

        // --- 1. Replace version directive ---
        // Handle #version 120, #version 330, #version 330 compatibility, etc.
        {
            size_t versionPos = source.find("#version");
            if (versionPos != std::string::npos)
            {
                size_t lineEnd = source.find('\n', versionPos);
                if (lineEnd == std::string::npos)
                    lineEnd = source.size();

                std::string replacement = "#version 300 es\n"
                                          "precision highp float;\n"
                                          "precision highp int;\n"
                                          "precision highp sampler2D;\n"
                                          "precision highp sampler2DShadow;\n"
                                          "precision highp samplerCube;\n";

                source.replace(versionPos, lineEnd - versionPos, replacement);
            }
        }

        // --- 2. Remove desktop-only extension directives ---
        {
            const std::string extensions[] = {
                "GL_ARB_uniform_buffer_object",
                "GL_EXT_gpu_shader4",
            };
            for (const auto& ext : extensions)
            {
                size_t pos = 0;
                while ((pos = source.find(ext, pos)) != std::string::npos)
                {
                    // Find the #extension line start
                    size_t lineStart = source.rfind('\n', pos);
                    lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
                    size_t lineEnd = source.find('\n', pos);
                    if (lineEnd == std::string::npos)
                        lineEnd = source.size();
                    else
                        lineEnd += 1;

                    // Only remove if this is an #extension directive
                    std::string line = source.substr(lineStart, lineEnd - lineStart);
                    if (line.find("#extension") != std::string::npos)
                    {
                        source.replace(lineStart, lineEnd - lineStart, "");
                        pos = lineStart;
                    }
                    else
                    {
                        pos += ext.size();
                    }
                }
            }
        }

        // --- 3. Remove #pragma import_defines(...) ---
        {
            size_t pos = 0;
            while ((pos = source.find("#pragma import_defines", pos)) != std::string::npos)
            {
                size_t lineEnd = source.find('\n', pos);
                if (lineEnd == std::string::npos)
                    lineEnd = source.size();
                else
                    lineEnd += 1;
                source.replace(pos, lineEnd - pos, "");
            }
        }

        // --- 4. Insert GLES3 preamble after the version/precision block ---
        // Find the end of the version block (after all precision statements)
        {
            size_t insertPos = 0;
            size_t precPos = source.rfind("precision highp");
            if (precPos != std::string::npos)
            {
                size_t lineEnd = source.find('\n', precPos);
                insertPos = (lineEnd == std::string::npos) ? source.size() : lineEnd + 1;
            }
            else
            {
                // After #version line
                size_t verPos = source.find("#version");
                if (verPos != std::string::npos)
                {
                    size_t lineEnd = source.find('\n', verPos);
                    insertPos = (lineEnd == std::string::npos) ? source.size() : lineEnd + 1;
                }
            }

            if (isVertex)
                source.insert(insertPos, sGLES3VertexPreamble);
            else if (isFragment)
                source.insert(insertPos, sGLES3FragmentPreamble);
        }

        // --- 5. Replace varying/attribute qualifiers ---
        if (isVertex)
        {
            // centroid varying → centroid out (must come before plain varying replacement)
            replaceAllSimple(source, "centroid varying", "centroid out");
            replaceAllWholeWord(source, "varying", "out");
            replaceAllWholeWord(source, "attribute", "in");
        }
        else if (isFragment)
        {
            replaceAllSimple(source, "centroid varying", "centroid in");
            replaceAllWholeWord(source, "varying", "in");
        }

        // --- 6. Replace deprecated vertex attribute builtins ---
        replaceAllWholeWord(source, "gl_Vertex", "osg_Vertex");
        replaceAllWholeWord(source, "gl_Normal", "osg_Normal");
        replaceAllWholeWord(source, "gl_Color", "osg_Color");
        replaceAllWholeWord(source, "gl_MultiTexCoord0", "osg_MultiTexCoord0");
        replaceAllWholeWord(source, "gl_MultiTexCoord1", "osg_MultiTexCoord1");
        replaceAllWholeWord(source, "gl_MultiTexCoord2", "osg_MultiTexCoord2");
        replaceAllWholeWord(source, "gl_MultiTexCoord3", "osg_MultiTexCoord3");
        replaceAllWholeWord(source, "gl_MultiTexCoord4", "osg_MultiTexCoord4");
        replaceAllWholeWord(source, "gl_MultiTexCoord5", "osg_MultiTexCoord5");
        replaceAllWholeWord(source, "gl_MultiTexCoord6", "osg_MultiTexCoord6");
        replaceAllWholeWord(source, "gl_MultiTexCoord7", "osg_MultiTexCoord7");

        // --- 7. Replace deprecated matrix builtins ---
        replaceAllWholeWord(source, "gl_ModelViewProjectionMatrix", "osg_ModelViewProjectionMatrix");
        replaceAllWholeWord(source, "gl_ModelViewMatrix", "osg_ModelViewMatrix");
        replaceAllWholeWord(source, "gl_ProjectionMatrix", "osg_ProjectionMatrix");
        replaceAllWholeWord(source, "gl_NormalMatrix", "osg_NormalMatrix");

        // --- 8. Replace gl_TextureMatrix[N] → omw_TextureMatrixN ---
        for (int i = 0; i < 8; ++i)
        {
            std::string from = "gl_TextureMatrix[" + std::to_string(i) + "]";
            std::string to = "omw_TextureMatrix" + std::to_string(i);
            replaceAllSimple(source, from, to);
        }

        // --- 9. Replace gl_FrontMaterial.field → omw_FrontMaterial.field ---
        replaceAllSimple(source, "gl_FrontMaterial.", "omw_FrontMaterial.");
        // Also handle gl_FrontMaterial as a standalone reference (e.g. passed as argument)
        replaceAllWholeWord(source, "gl_FrontMaterial", "omw_FrontMaterial");

        // --- 10. Replace gl_Fog.field → omw_Fog.field ---
        replaceAllSimple(source, "gl_Fog.", "omw_Fog.");
        replaceAllWholeWord(source, "gl_Fog", "omw_Fog");

        // --- 11. Replace gl_FragData[N] → omw_FragDataN ---
        for (int i = 0; i < 4; ++i)
        {
            std::string from = "gl_FragData[" + std::to_string(i) + "]";
            std::string to = "omw_FragData" + std::to_string(i);
            replaceAllSimple(source, from, to);
        }

        // --- 12. Replace gl_FragColor → omw_FragData0 (unavailable in ES 3.00) ---
        if (isFragment)
            replaceAllWholeWord(source, "gl_FragColor", "omw_FragData0");

        // --- 13. Remove gl_ClipVertex (no ES3 equivalent) ---
        removeClipVertexLines(source);

        // --- 14. Replace texture lookup functions ---
        // GLSL 300 es uses texture() instead of texture2D(), textureCube(), etc.
        replaceAllWholeWord(source, "texture2D", "texture");
        replaceAllWholeWord(source, "texture3D", "texture");
        replaceAllWholeWord(source, "textureCube", "texture");
        replaceAllWholeWord(source, "shadow2DProj", "textureProj");
    }
#endif // __EMSCRIPTEN__
}

namespace Shader
{
    struct HotReloadManager
    {
        using KeysHolder = std::set<ShaderManager::MapKey>;

        std::unordered_map<std::string, KeysHolder> mShaderFiles;
        std::unordered_map<std::string, std::set<std::filesystem::path>> templateIncludedFiles;
        std::filesystem::file_time_type mLastAutoRecompileTime;
        bool mHotReloadEnabled;
        bool mTriggerReload;

        HotReloadManager()
        {
            mTriggerReload = false;
            mHotReloadEnabled = false;
            mLastAutoRecompileTime = std::filesystem::file_time_type::clock::now();
        }

        void addShaderFiles(const std::string& templateName, const ShaderManager::DefineMap& defines)
        {
            const std::set<std::filesystem::path>& shaderFiles = templateIncludedFiles[templateName];
            for (const std::filesystem::path& file : shaderFiles)
            {
                mShaderFiles[Files::pathToUnicodeString(file)].insert(std::make_pair(templateName, defines));
            }
        }

        void update(ShaderManager& manager, osgViewer::Viewer& viewer)
        {
            auto timeSinceLastCheckMillis = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::filesystem::file_time_type::clock::now() - mLastAutoRecompileTime);
            if ((mHotReloadEnabled && timeSinceLastCheckMillis.count() > 200) || mTriggerReload == true)
            {
                reloadTouchedShaders(manager, viewer);
            }
            mTriggerReload = false;
        }

        void reloadTouchedShaders(ShaderManager& manager, osgViewer::Viewer& viewer)
        {
            bool threadsRunningToStop = false;
            for (auto& [pathShaderToTest, shaderKeys] : mShaderFiles)
            {
                const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(pathShaderToTest);
                if (writeTime.time_since_epoch() > mLastAutoRecompileTime.time_since_epoch())
                {
                    if (!threadsRunningToStop)
                    {
                        threadsRunningToStop = viewer.areThreadsRunning();
                        if (threadsRunningToStop)
                            viewer.stopThreading();
                    }

                    for (const auto& [templateName, shaderDefines] : shaderKeys)
                    {
                        ShaderManager::ShaderMap::iterator shaderIt
                            = manager.mShaders.find(std::make_pair(templateName, shaderDefines));
                        if (shaderIt == manager.mShaders.end())
                        {
                            Log(Debug::Error) << "Failed to find shader " << templateName;
                            continue;
                        }

                        ShaderManager::TemplateMap::iterator templateIt = manager.mShaderTemplates.find(
                            templateName); // Can't be Null, if we're here it means the template was added
                        assert(templateIt != manager.mShaderTemplates.end());
                        std::string& shaderSource = templateIt->second;
                        std::set<std::filesystem::path> insertedPaths;
                        std::filesystem::path path = (std::filesystem::path(manager.mPath) / templateName);
                        std::ifstream stream;
                        stream.open(path);
                        if (stream.fail())
                        {
                            Log(Debug::Error)
                                << "Failed to open " << path << ": " << std::generic_category().message(errno);
                            continue;
                        }
                        std::stringstream buffer;
                        buffer << stream.rdbuf();

                        // parse includes
                        int fileNumber = 1;
                        std::string source = buffer.str();
                        if (!addLineDirectivesAfterConditionalBlocks(source)
                            || !parseIncludes(std::filesystem::path(manager.mPath), source, templateName, fileNumber,
                                {}, insertedPaths))
                        {
                            break;
                        }
                        shaderSource = std::move(source);

                        std::vector<std::string> linkedShaderNames;
                        if (!manager.createSourceFromTemplate(
                                shaderSource, linkedShaderNames, templateName, shaderDefines))
                        {
                            break;
                        }
#ifdef __EMSCRIPTEN__
                        transformShaderToGLES3(shaderSource, shaderIt->second->getType());
#endif
                        shaderIt->second->setShaderSource(shaderSource);
                    }
                }
            }
            if (threadsRunningToStop)
                viewer.startThreading();
            mLastAutoRecompileTime = std::filesystem::file_time_type::clock::now();
        }
    };

    ShaderManager::ShaderManager()
    {
        mHotReloadManager = std::make_unique<HotReloadManager>();
    }

    ShaderManager::~ShaderManager() = default;

    void ShaderManager::setShaderPath(const std::filesystem::path& path)
    {
        mPath = path;
    }

    bool parseForeachDirective(std::string& source, const std::string& templateName, size_t foundPos)
    {
        constexpr std::string_view directiveStart = "$foreach";
        size_t iterNameStart = foundPos + directiveStart.size() + 1;
        size_t iterNameEnd = source.find_first_of(" \n\r()[].;,", iterNameStart);
        if (iterNameEnd == std::string::npos)
        {
            Log(Debug::Error) << "Shader " << templateName << " error: Unexpected EOF";
            return false;
        }
        std::string iteratorName = "$" + source.substr(iterNameStart, iterNameEnd - iterNameStart);

        size_t listStart = iterNameEnd + 1;
        size_t listEnd = source.find_first_of("\n\r", listStart);
        if (listEnd == std::string::npos)
        {
            Log(Debug::Error) << "Shader " << templateName << " error: Unexpected EOF";
            return false;
        }
        std::string_view list = std::string_view(source).substr(listStart, listEnd - listStart);
        std::vector<std::string> listElements;
        if (!list.empty())
            Misc::StringUtils::split(list, listElements, ",");

        size_t contentStart = source.find_first_not_of("\n\r", listEnd);
        constexpr std::string_view directiveEnd = "$endforeach";
        size_t contentEnd = source.find(directiveEnd, contentStart);
        if (contentEnd == std::string::npos)
        {
            Log(Debug::Error) << "Shader " << templateName << " error: Unexpected EOF";
            return false;
        }
        std::string_view content = std::string_view(source).substr(contentStart, contentEnd - contentStart);

        size_t overallEnd = contentEnd + directiveEnd.size();

        int lineNumber = getLineNumber(source, overallEnd, 2, 0);

        std::string replacement;
        for (const std::string& element : listElements)
        {
            std::string contentInstance(content);
            size_t foundIterator;
            while ((foundIterator = contentInstance.find(iteratorName)) != std::string::npos)
                contentInstance.replace(foundIterator, iteratorName.length(), element);
            replacement += contentInstance;
        }
        replacement += "\n#line " + std::to_string(lineNumber);
        source.replace(foundPos, overallEnd - foundPos, replacement);
        return true;
    }

    bool parseLinkDirective(
        std::string& source, std::string& linkTarget, const std::string& templateName, size_t foundPos)
    {
        size_t endPos = foundPos + 5;
        size_t lineEnd = source.find_first_of('\n', endPos);
        // If lineEnd = npos, this is the last line, so no need to check
        std::string linkStatement = source.substr(endPos, lineEnd - endPos);
        std::regex linkRegex(R"r(\s*"([^"]+)"\s*)r" // Find any quoted string as the link name -> match[1]
                             R"r((if\s+)r" // Begin optional condition -> match[2]
                             R"r((!)?\s*)r" // Optional ! -> match[3]
                             R"r(([_a-zA-Z0-9]+)?)r" // The condition -> match[4]
                             R"r()?\s*)r" // End optional condition -> match[2]
        );
        std::smatch linkMatch;
        bool hasCondition = false;
        std::string linkConditionExpression;
        if (std::regex_match(linkStatement, linkMatch, linkRegex))
        {
            linkTarget = linkMatch[1].str();
            hasCondition = !linkMatch[2].str().empty();
            linkConditionExpression = linkMatch[4].str();
        }
        else
        {
            Log(Debug::Error) << "Shader " << templateName << " error: Expected a shader filename to link";
            return false;
        }
        if (linkTarget.empty())
        {
            Log(Debug::Error) << "Shader " << templateName << " error: Empty link name";
            return false;
        }

        if (hasCondition)
        {
            bool condition = !(linkConditionExpression.empty() || linkConditionExpression == "0");
            if (linkMatch[3].str() == "!")
                condition = !condition;

            if (!condition)
                linkTarget.clear();
        }

        source.replace(foundPos, lineEnd - foundPos, "");
        return true;
    }

    bool parseDirectives(std::string& source, std::vector<std::string>& linkedShaderTemplateNames,
        const ShaderManager::DefineMap& defines, const ShaderManager::DefineMap& globalDefines,
        const std::string& templateName)
    {
        const char escapeCharacter = '$';
        size_t foundPos = 0;

        while ((foundPos = source.find(escapeCharacter, foundPos)) != std::string::npos)
        {
            size_t endPos = source.find_first_of(" \n\r()[].;,", foundPos);
            if (endPos == std::string::npos)
            {
                Log(Debug::Error) << "Shader " << templateName << " error: Unexpected EOF";
                return false;
            }
            std::string_view directive = std::string_view(source).substr(foundPos + 1, endPos - (foundPos + 1));
            if (directive == "foreach")
            {
                if (!parseForeachDirective(source, templateName, foundPos))
                    return false;
            }
            else if (directive == "link")
            {
                std::string linkTarget;
                if (!parseLinkDirective(source, linkTarget, templateName, foundPos))
                    return false;
                if (!linkTarget.empty() && linkTarget != templateName)
                    linkedShaderTemplateNames.push_back(std::move(linkTarget));
            }
            else
            {
                Log(Debug::Error) << "Shader " << templateName << " error: Unknown shader directive: $" << directive;
                return false;
            }
        }

        return true;
    }

    bool parseDefines(std::string& source, const ShaderManager::DefineMap& defines,
        const ShaderManager::DefineMap& globalDefines, const std::string& templateName)
    {
        const char escapeCharacter = '@';
        size_t foundPos = 0;
        std::vector<std::string> forIterators;
        while ((foundPos = source.find(escapeCharacter)) != std::string::npos)
        {
            size_t endPos = source.find_first_of(" \n\r()[].;,", foundPos);
            if (endPos == std::string::npos)
            {
                Log(Debug::Error) << "Shader " << templateName << " error: Unexpected EOF";
                return false;
            }
            std::string define = source.substr(foundPos + 1, endPos - (foundPos + 1));
            ShaderManager::DefineMap::const_iterator defineFound = defines.find(define);
            ShaderManager::DefineMap::const_iterator globalDefineFound = globalDefines.find(define);
            if (define == "foreach")
            {
                source.replace(foundPos, 1, "$");
                size_t iterNameStart = endPos + 1;
                size_t iterNameEnd = source.find_first_of(" \n\r()[].;,", iterNameStart);
                if (iterNameEnd == std::string::npos)
                {
                    Log(Debug::Error) << "Shader " << templateName << " error: Unexpected EOF";
                    return false;
                }
                forIterators.push_back(source.substr(iterNameStart, iterNameEnd - iterNameStart));
            }
            else if (define == "endforeach")
            {
                source.replace(foundPos, 1, "$");
                if (forIterators.empty())
                {
                    Log(Debug::Error) << "Shader " << templateName << " error: endforeach without foreach";
                    return false;
                }
                else
                    forIterators.pop_back();
            }
            else if (define == "link")
            {
                source.replace(foundPos, 1, "$");
            }
            else if (std::find(forIterators.begin(), forIterators.end(), define) != forIterators.end())
            {
                source.replace(foundPos, 1, "$");
            }
            else if (defineFound != defines.end())
            {
                source.replace(foundPos, endPos - foundPos, defineFound->second);
            }
            else if (globalDefineFound != globalDefines.end())
            {
                source.replace(foundPos, endPos - foundPos, globalDefineFound->second);
            }
            else
            {
                Log(Debug::Error) << "Shader " << templateName << " error: Undefined " << define;
                return false;
            }
        }
        return true;
    }

    osg::ref_ptr<osg::Shader> ShaderManager::getShader(
        std::string templateName, const ShaderManager::DefineMap& defines, std::optional<osg::Shader::Type> type)
    {
        std::unique_lock<std::mutex> lock(mMutex);

        // TODO: Implement mechanism to switch to core or compatibility profile shaders.
        // This logic is temporary until core support is supported.
        if (getRootPrefix(templateName).empty())
            templateName = "compatibility/" + templateName;

        // read the template if we haven't already
        TemplateMap::iterator templateIt = mShaderTemplates.find(templateName);
        std::set<std::filesystem::path> insertedPaths;

        if (templateIt == mShaderTemplates.end())
        {
            std::filesystem::path path = mPath / templateName;
            std::ifstream stream;
            stream.open(path);
            if (stream.fail())
            {
                Log(Debug::Error) << "Failed to open shader " << path << ": " << std::generic_category().message(errno);
                return nullptr;
            }
            std::stringstream buffer;
            buffer << stream.rdbuf();

            // parse includes
            int fileNumber = 1;
            std::string source = buffer.str();
            if (!addLineDirectivesAfterConditionalBlocks(source)
                || !parseIncludes(mPath, source, templateName, fileNumber, {}, insertedPaths))
                return nullptr;
            mHotReloadManager->templateIncludedFiles[templateName] = std::move(insertedPaths);
            templateIt = mShaderTemplates.insert(std::make_pair(templateName, source)).first;
        }

        ShaderMap::iterator shaderIt = mShaders.find(std::make_pair(templateName, defines));
        if (shaderIt == mShaders.end())
        {
            std::string shaderSource = templateIt->second;
            std::vector<std::string> linkedShaderNames;
            if (!createSourceFromTemplate(shaderSource, linkedShaderNames, templateName, defines))
            {
                // Add to the cache anyway to avoid logging the same error over and over.
                mShaders.insert(std::make_pair(std::make_pair(templateName, defines), nullptr));
                return nullptr;
            }

            osg::Shader::Type resolvedType = type ? *type : getShaderType(templateName);

#ifdef __EMSCRIPTEN__
            // Transform GLSL 1.20 compatibility profile shaders to GLSL ES 3.00 for WebGL 2.0
            transformShaderToGLES3(shaderSource, resolvedType);
#endif

            osg::ref_ptr<osg::Shader> shader(new osg::Shader(resolvedType));
            shader->setShaderSource(shaderSource);
            // Assign a unique prefix to allow the SharedStateManager to compare shaders efficiently.
            // Append shader source filename for debugging.
            static unsigned int counter = 0;
            shader->setName(std::format("{} {}", counter++, templateName));

            mHotReloadManager->addShaderFiles(templateName, defines);

            lock.unlock();
            getLinkedShaders(shader, linkedShaderNames, defines);
            lock.lock();

            shaderIt = mShaders.insert(std::make_pair(std::make_pair(templateName, defines), shader)).first;
        }
        return shaderIt->second;
    }

    osg::ref_ptr<osg::Program> ShaderManager::getProgram(
        const std::string& templateName, const DefineMap& defines, const osg::Program* programTemplate)
    {
        auto vert = getShader(templateName + ".vert", defines);
        auto frag = getShader(templateName + ".frag", defines);

        if (!vert || !frag)
            throw std::runtime_error("failed initializing shader: " + templateName);

        return getProgram(std::move(vert), std::move(frag), programTemplate);
    }

    osg::ref_ptr<osg::Program> ShaderManager::getProgram(osg::ref_ptr<osg::Shader> vertexShader,
        osg::ref_ptr<osg::Shader> fragmentShader, const osg::Program* programTemplate)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        ProgramMap::iterator found = mPrograms.find(std::make_pair(vertexShader, fragmentShader));
        if (found == mPrograms.end())
        {
            if (!programTemplate)
                programTemplate = mProgramTemplate;
            osg::ref_ptr<osg::Program> program
                = programTemplate ? cloneProgram(programTemplate) : osg::ref_ptr<osg::Program>(new osg::Program);
            program->addShader(vertexShader);
            program->addShader(fragmentShader);
            addLinkedShaders(vertexShader, program);
            addLinkedShaders(fragmentShader, program);

            found = mPrograms.insert(std::make_pair(std::make_pair(vertexShader, fragmentShader), program)).first;
        }
        return found->second;
    }

    osg::ref_ptr<osg::Program> ShaderManager::cloneProgram(const osg::Program* src)
    {
        osg::ref_ptr<osg::Program> program = static_cast<osg::Program*>(src->clone(osg::CopyOp::SHALLOW_COPY));
        for (auto& [name, idx] : src->getUniformBlockBindingList())
            program->addBindUniformBlock(name, idx);
        return program;
    }

    ShaderManager::DefineMap ShaderManager::getGlobalDefines()
    {
        return DefineMap(mGlobalDefines);
    }

    void ShaderManager::setGlobalDefines(DefineMap& globalDefines)
    {
        mGlobalDefines = globalDefines;
        for (const auto& [key, shader] : mShaders)
        {
            std::string templateId = key.first;
            ShaderManager::DefineMap defines = key.second;
            if (shader == nullptr)
                // I'm not sure how to handle a shader that was already broken as there's no way to get a potential
                // replacement to the nodes that need it.
                continue;
            std::string shaderSource = mShaderTemplates[templateId];
            std::vector<std::string> linkedShaderNames;
            if (!createSourceFromTemplate(shaderSource, linkedShaderNames, templateId, defines))
                // We just broke the shader and there's no way to force existing objects back to fixed-function mode as
                // we would when creating the shader. If we put a nullptr in the shader map, we just lose the ability to
                // put a working one in later.
                continue;
#ifdef __EMSCRIPTEN__
            transformShaderToGLES3(shaderSource, shader->getType());
#endif
            shader->setShaderSource(shaderSource);

            getLinkedShaders(shader, linkedShaderNames, defines);
        }
    }

    void ShaderManager::releaseGLObjects(osg::State* state)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        for (const auto& [_, shader] : mShaders)
        {
            if (shader != nullptr)
                shader->releaseGLObjects(state);
        }
        for (const auto& [_, program] : mPrograms)
            program->releaseGLObjects(state);
    }

    bool ShaderManager::createSourceFromTemplate(std::string& source,
        std::vector<std::string>& linkedShaderTemplateNames, const std::string& templateName,
        const ShaderManager::DefineMap& defines)
    {
        if (!parseDefines(source, defines, mGlobalDefines, templateName))
            return false;
        if (!parseDirectives(source, linkedShaderTemplateNames, defines, mGlobalDefines, templateName))
            return false;
        return true;
    }

    void ShaderManager::getLinkedShaders(
        osg::ref_ptr<osg::Shader> shader, const std::vector<std::string>& linkedShaderNames, const DefineMap& defines)
    {
        mLinkedShaders.erase(shader);
        if (linkedShaderNames.empty())
            return;

        for (auto& linkedShaderName : linkedShaderNames)
        {
            auto linkedShader = getShader(linkedShaderName, defines, shader->getType());
            if (linkedShader)
                mLinkedShaders[shader].emplace_back(linkedShader);
        }
    }

    void ShaderManager::addLinkedShaders(osg::ref_ptr<osg::Shader> shader, osg::ref_ptr<osg::Program> program)
    {
        auto linkedIt = mLinkedShaders.find(shader);
        if (linkedIt != mLinkedShaders.end())
            for (const auto& linkedShader : linkedIt->second)
                program->addShader(linkedShader);
    }

    int ShaderManager::reserveGlobalTextureUnits(Slot slot, int count)
    {
        // TODO: Reuse units when count increase forces reallocation
        // TODO: Warn if trampling on the ~8 units needed by model textures
        auto unit = mReservedTextureUnitsBySlot[static_cast<int>(slot)];
        if (unit.index >= 0 && unit.count >= count)
            return unit.index;

        if (getAvailableTextureUnits() < count + 1)
            throw std::runtime_error("Can't reserve texture unit; no available units");
        mReservedTextureUnits += count;

        unit.index = mMaxTextureUnits - mReservedTextureUnits;
        unit.count = count;

        mReservedTextureUnitsBySlot[static_cast<int>(slot)] = unit;

        std::string_view slotDescr;
        switch (slot)
        {
            case Slot::OpaqueDepthTexture:
                slotDescr = "opaque depth texture";
                break;
            case Slot::SkyTexture:
                slotDescr = "sky RTT";
                break;
            case Slot::ShadowMaps:
                slotDescr = "shadow maps";
                break;
            default:
                slotDescr = "UNKNOWN";
        }
        if (unit.count == 1)
            Log(Debug::Info) << "Reserving texture unit for " << slotDescr << ": " << unit.index;
        else
            Log(Debug::Info) << "Reserving texture units for " << slotDescr << ": " << unit.index << ".."
                             << (unit.index + count - 1);

        return unit.index;
    }

    void ShaderManager::update(osgViewer::Viewer& viewer)
    {
        mHotReloadManager->update(*this, viewer);
    }

    void ShaderManager::setHotReloadEnabled(bool value)
    {
        mHotReloadManager->mHotReloadEnabled = value;
    }

    void ShaderManager::triggerShaderReload()
    {
        mHotReloadManager->mTriggerReload = true;
    }

}
