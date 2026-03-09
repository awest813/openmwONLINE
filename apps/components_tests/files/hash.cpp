#include <components/files/constrainedfilestream.hpp>
#include <components/files/conversion.hpp>
#include <components/files/hash.hpp>
#include <components/testing/util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <streambuf>
#include <string>

namespace
{
    using namespace testing;
    using namespace TestingOpenMW;
    using namespace Files;

    struct Params
    {
        std::size_t mSize;
        std::array<std::uint64_t, 2> mHash;
    };

    struct FilesGetHash : TestWithParam<Params>
    {
    };

    TEST(FilesGetHash, shouldClearErrors)
    {
        const auto fileName = outputFilePath("fileName");
        std::string content;
        std::fill_n(std::back_inserter(content), 1, 'a');
        std::istringstream stream(content);
        stream.exceptions(std::ios::failbit | std::ios::badbit);
        EXPECT_THAT(getHash(Files::pathToUnicodeString(fileName), stream),
            ElementsAre(9607679276477937801ull, 16624257681780017498ull));
    }

    TEST(FilesGetHash, shouldRestoreStreamPosition)
    {
        const auto fileName = outputFilePath("fileName");
        std::string content = "abcdefgh";
        std::istringstream stream(content);
        stream.seekg(4);
        getHash(Files::pathToUnicodeString(fileName), stream);
        EXPECT_EQ(stream.tellg(), std::streampos(4));
    }

    struct FailingBuffer : std::streambuf
    {
        int_type underflow() override { throw std::runtime_error("Read error"); }
        std::streamsize xsgetn(char*, std::streamsize) override { throw std::runtime_error("Read error"); }
    };

    TEST(FilesGetHash, shouldThrowOnReadError)
    {
        const std::string fileName = "testFile.txt";
        FailingBuffer buffer;
        std::istream stream(&buffer);
        EXPECT_THROW(
            {
                try
                {
                    getHash(fileName, stream);
                }
                catch (const std::runtime_error& e)
                {
                    EXPECT_THAT(e.what(), HasSubstr(fileName));
                    EXPECT_THAT(e.what(), HasSubstr("Read error"));
                    throw;
                }
            },
            std::runtime_error);
    }

    TEST_P(FilesGetHash, shouldReturnHashForStringStream)
    {
        const auto fileName = outputFilePath("fileName");
        std::string content;
        std::fill_n(std::back_inserter(content), GetParam().mSize, 'a');
        std::istringstream stream(content);
        EXPECT_EQ(getHash(Files::pathToUnicodeString(fileName), stream), GetParam().mHash);
    }

    TEST_P(FilesGetHash, shouldReturnHashForConstrainedFileStream)
    {
        std::string fileName(UnitTest::GetInstance()->current_test_info()->name());
        std::replace(fileName.begin(), fileName.end(), '/', '_');
        std::string content;
        std::fill_n(std::back_inserter(content), GetParam().mSize, 'a');
        const auto file = outputFilePath(fileName);
        std::fstream(file, std::ios_base::out | std::ios_base::binary)
            .write(content.data(), static_cast<std::streamsize>(content.size()));
        const auto stream = Files::openConstrainedFileStream(file, 0, content.size());
        EXPECT_EQ(getHash(Files::pathToUnicodeString(file), *stream), GetParam().mHash);
    }

    INSTANTIATE_TEST_SUITE_P(Params, FilesGetHash,
        Values(Params{ 0, { 0, 0 } }, Params{ 1, { 9607679276477937801ull, 16624257681780017498ull } },
            Params{ 128, { 15287858148353394424ull, 16818615825966581310ull } },
            Params{ 1000, { 11018119256083894017ull, 6631144854802791578ull } },
            Params{ 4096, { 11972283295181039100ull, 16027670129106775155ull } },
            Params{ 4097, { 16717956291025443060ull, 12856404199748778153ull } },
            Params{ 5000, { 15775925571142117787ull, 10322955217889622896ull } }));
}
