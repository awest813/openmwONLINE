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
    // Returns true if the character at `pos` is a word boundary (not alphanumeric or underscore)
    bool isWordBoundary(const std::string& s, size_t pos)
    {
        if (pos >= s.size())
            return true;
        char c = s[pos];
        return !(std::isalnum(static_cast<unsigned char>(c)) || c == '_');
    }

    // Replace a GLSL identifier with word-boundary awareness to avoid false matches.
    // E.g., replacing "gl_ModelViewMatrix" won't match "gl_ModelViewProjectionMatrix".
    void replaceGLSLIdentifier(std::string& source, const std::string& from, const std::string& to)
    {
        size_t pos = 0;
        while ((pos = source.find(from, pos)) != std::string::npos)
        {
            if (pos > 0 && !isWordBoundary(source, pos - 1))
            {
                pos += from.size();
                continue;
            }
            if (!isWordBoundary(source, pos + from.size()))
            {
                pos += from.size();
                continue;
            }
            source.replace(pos, from.size(), to);
            pos += to.size();
        }
    }

    // Check if source contains a whole-word identifier
    bool containsGLSLIdentifier(const std::string& source, const std::string& id)
    {
        size_t pos = 0;
        while ((pos = source.find(id, pos)) != std::string::npos)
        {
            if ((pos == 0 || isWordBoundary(source, pos - 1)) && isWordBoundary(source, pos + id.size()))
                return true;
            pos += id.size();
        }
        return false;
    }

    // Simple replace all (for non-identifier patterns like "texture2D(" or "gl_FragData[0]")
    void replaceAllSubstrings(std::string& source, const std::string& from, const std::string& to)
    {
        size_t pos = 0;
        while ((pos = source.find(from, pos)) != std::string::npos)
        {
            source.replace(pos, from.size(), to);
            pos += to.size();
        }
    }

#ifdef __EMSCRIPTEN__
    // Convert GLSL 1.20 (desktop compatibility) shader source to GLSL ES 3.00 (WebGL 2.0).
    // This allows all existing compatibility/ shaders to run on Emscripten/WebGL without
    // maintaining a separate set of shader files.
    //
    // OSG compiled with OPENGL_PROFILE=GLES3 provides built-in uniform emulation:
    //   osg_ModelViewMatrix, osg_ProjectionMatrix, osg_NormalMatrix, osg_FrontMaterial, etc.
    // and vertex attribute bindings:
    //   osg_Vertex (loc 0), osg_Normal (loc 2), osg_Color (loc 3), osg_MultiTexCoordN (loc 8+N)
    //
    std::string convertToGLSLES300(const std::string& source, osg::Shader::Type type)
    {
        std::string result = source;
        bool isVertex = (type == osg::Shader::VERTEX);
        bool isFragment = (type == osg::Shader::FRAGMENT);

        // --- 1. Replace #version line with ES 3.00 + precision qualifiers ---
        {
            size_t versionPos = result.find("#version ");
            if (versionPos != std::string::npos)
            {
                size_t lineEnd = result.find('\n', versionPos);
                if (lineEnd == std::string::npos)
                    lineEnd = result.size();
                result.replace(versionPos, lineEnd - versionPos,
                    "#version 300 es\n"
                    "precision highp float;\n"
                    "precision highp int;\n"
                    "precision highp sampler2D;\n"
                    "precision highp sampler3D;\n"
                    "precision highp samplerCube;\n"
                    "precision highp sampler2DShadow;");
            }
        }

        // --- 2. Remove/comment out desktop-only extension directives ---
        // UBO and gpu_shader4 features are built-in to ES 3.0
        replaceAllSubstrings(result,
            "#extension GL_ARB_uniform_buffer_object : require",
            "// GL_ARB_uniform_buffer_object (built-in in ES 3.0)");
        replaceAllSubstrings(result,
            "#extension GL_EXT_gpu_shader4: require",
            "// GL_EXT_gpu_shader4 (built-in in ES 3.0)");
        replaceAllSubstrings(result,
            "#extension GL_EXT_gpu_shader4 : require",
            "// GL_EXT_gpu_shader4 (built-in in ES 3.0)");
        replaceAllSubstrings(result,
            "#extension GL_EXT_texture_array : require",
            "// GL_EXT_texture_array (built-in in ES 3.0)");

        // --- 3. Replace varying/attribute keywords based on shader type ---
        // Handle 'centroid varying' first to avoid partial replacement
        if (isVertex)
        {
            replaceGLSLIdentifier(result, "centroid varying", "centroid out");
            replaceGLSLIdentifier(result, "varying", "out");
            replaceGLSLIdentifier(result, "attribute", "in");
        }
        else if (isFragment)
        {
            replaceGLSLIdentifier(result, "centroid varying", "centroid in");
            replaceGLSLIdentifier(result, "varying", "in");
        }

        // --- 4. Replace deprecated texture lookup functions ---
        replaceAllSubstrings(result, "texture2D(", "texture(");
        replaceAllSubstrings(result, "texture3D(", "texture(");
        replaceAllSubstrings(result, "textureCube(", "texture(");
        replaceAllSubstrings(result, "shadow2DProj(", "textureProj(");
        replaceAllSubstrings(result, "shadow2D(", "texture(");

        // --- 5. Build declarations for gl_ built-in replacements ---
        std::string declarations;

        // Vertex attributes (only in vertex shaders)
        if (isVertex)
        {
            if (containsGLSLIdentifier(result, "gl_Vertex"))
            {
                replaceGLSLIdentifier(result, "gl_Vertex", "osg_Vertex");
                declarations += "in vec4 osg_Vertex;\n";
            }
            if (containsGLSLIdentifier(result, "gl_Normal"))
            {
                replaceGLSLIdentifier(result, "gl_Normal", "osg_Normal");
                declarations += "in vec3 osg_Normal;\n";
            }
            if (containsGLSLIdentifier(result, "gl_Color"))
            {
                replaceGLSLIdentifier(result, "gl_Color", "osg_Color");
                declarations += "in vec4 osg_Color;\n";
            }
            for (int i = 0; i < 8; ++i)
            {
                std::string glName = "gl_MultiTexCoord" + std::to_string(i);
                if (containsGLSLIdentifier(result, glName))
                {
                    std::string osgName = "osg_MultiTexCoord" + std::to_string(i);
                    replaceGLSLIdentifier(result, glName, osgName);
                    declarations += "in vec4 " + osgName + ";\n";
                }
            }
        }

        // Built-in matrices → OSG uniform equivalents
        if (containsGLSLIdentifier(result, "gl_ModelViewProjectionMatrix"))
        {
            replaceGLSLIdentifier(result, "gl_ModelViewProjectionMatrix", "osg_ModelViewProjectionMatrix");
            declarations += "uniform mat4 osg_ModelViewProjectionMatrix;\n";
        }
        if (containsGLSLIdentifier(result, "gl_ModelViewMatrix"))
        {
            replaceGLSLIdentifier(result, "gl_ModelViewMatrix", "osg_ModelViewMatrix");
            declarations += "uniform mat4 osg_ModelViewMatrix;\n";
        }
        if (containsGLSLIdentifier(result, "gl_ProjectionMatrix"))
        {
            replaceGLSLIdentifier(result, "gl_ProjectionMatrix", "osg_ProjectionMatrix");
            declarations += "uniform mat4 osg_ProjectionMatrix;\n";
        }
        if (containsGLSLIdentifier(result, "gl_NormalMatrix"))
        {
            replaceGLSLIdentifier(result, "gl_NormalMatrix", "osg_NormalMatrix");
            declarations += "uniform mat3 osg_NormalMatrix;\n";
        }
        if (containsGLSLIdentifier(result, "gl_TextureMatrix"))
        {
            replaceGLSLIdentifier(result, "gl_TextureMatrix", "osg_TextureMatrix");
            declarations += "uniform mat4 osg_TextureMatrix[8];\n";
        }

        // Material struct → OSG uniform equivalent
        if (containsGLSLIdentifier(result, "gl_FrontMaterial"))
        {
            replaceGLSLIdentifier(result, "gl_FrontMaterial", "osg_FrontMaterial");
            declarations +=
                "struct osg_MaterialParameters {\n"
                "    vec4 ambient;\n"
                "    vec4 diffuse;\n"
                "    vec4 specular;\n"
                "    vec4 emission;\n"
                "    float shininess;\n"
                "};\n"
                "uniform osg_MaterialParameters osg_FrontMaterial;\n";
        }

        // Light model → OSG uniform equivalent (scene ambient)
        if (containsGLSLIdentifier(result, "gl_LightModel"))
        {
            replaceGLSLIdentifier(result, "gl_LightModel", "osg_LightModel");
            declarations +=
                "struct osg_LightModelParameters {\n"
                "    vec4 ambient;\n"
                "};\n"
                "uniform osg_LightModelParameters osg_LightModel;\n";
        }

        // --- 6. Fragment outputs ---
        if (isFragment)
        {
            bool addedFragColor = false;
            if (result.find("gl_FragData[0]") != std::string::npos)
            {
                replaceAllSubstrings(result, "gl_FragData[0]", "osg_FragColor");
                declarations += "layout(location = 0) out vec4 osg_FragColor;\n";
                addedFragColor = true;
            }
            if (result.find("gl_FragData[1]") != std::string::npos)
            {
                replaceAllSubstrings(result, "gl_FragData[1]", "osg_FragData1");
                declarations += "layout(location = 1) out vec4 osg_FragData1;\n";
            }
            if (result.find("gl_FragColor") != std::string::npos)
            {
                replaceAllSubstrings(result, "gl_FragColor", "osg_FragColor");
                if (!addedFragColor)
                    declarations += "layout(location = 0) out vec4 osg_FragColor;\n";
            }
        }

        // --- 7. Remove gl_ClipVertex assignments (not available in ES 3.0) ---
        {
            size_t pos = 0;
            while ((pos = result.find("gl_ClipVertex", pos)) != std::string::npos)
            {
                size_t stmtStart = result.rfind('\n', pos);
                stmtStart = (stmtStart == std::string::npos) ? 0 : stmtStart + 1;
                size_t stmtEnd = result.find(';', pos);
                if (stmtEnd != std::string::npos)
                {
                    stmtEnd++; // include the semicolon
                    result.replace(stmtStart, stmtEnd - stmtStart,
                        "// gl_ClipVertex removed (not available in ES 3.0)");
                    pos = stmtStart + 52;
                }
                else
                    break;
            }
        }

        // --- 8. Insert declarations after the version/precision block ---
        if (!declarations.empty())
        {
            // Find the end of the #version + precision block
            size_t insertPos = 0;
            size_t searchFrom = 0;
            while (true)
            {
                size_t lineStart = result.find_first_not_of(" \t\n\r", searchFrom);
                if (lineStart == std::string::npos)
                    break;
                if (result.compare(lineStart, 10, "precision ") == 0
                    || result.compare(lineStart, 9, "#version ") == 0)
                {
                    size_t lineEnd = result.find('\n', lineStart);
                    insertPos = (lineEnd != std::string::npos) ? lineEnd + 1 : result.size();
                    searchFrom = insertPos;
                }
                else
                    break;
            }

            result.insert(insertPos, "\n" + declarations + "\n");
        }

        return result;
    }
#endif // __EMSCRIPTEN__
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
                        shaderSource = convertToGLSLES300(
                            shaderSource, getShaderType(templateName));
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

            osg::Shader::Type shaderType = type ? *type : getShaderType(templateName);

#ifdef __EMSCRIPTEN__
            shaderSource = convertToGLSLES300(shaderSource, shaderType);
#endif

            osg::ref_ptr<osg::Shader> shader(new osg::Shader(shaderType));
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
            shaderSource = convertToGLSLES300(shaderSource, shader->getType());
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
