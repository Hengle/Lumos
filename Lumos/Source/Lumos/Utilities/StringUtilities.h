#pragma once
#include "Core/String.h"
#include "Core/DataStructures/TDArray.h"
#include <string>

#ifdef LUMOS_PLATFORM_ANDROID
template <typename T>
std::string to_string(const T& n)
{
    std::ostringstream stm;
    stm << n;
    return stm.str();
}
#endif

namespace Lumos
{
    namespace StringUtilities
    {
        template <typename T>
        static std::string ToString(const T& input)
        {
#ifdef LUMOS_PLATFORM_ANDROID
            return to_string(input);
#else
            return std::to_string(input);
#endif
        }

        std::string GetFilePathExtension(const std::string& FileName);
        std::string GetFileName(const std::string& FilePath);
        std::string GetFileLocation(const std::string& FilePath);

        std::string RemoveName(const std::string& FilePath);
        std::string RemoveFilePathExtension(const std::string& FileName);
        std::string ToLower(const std::string& text);

        std::string& BackSlashesToSlashes(std::string& string);
        std::string& SlashesToBackSlashes(std::string& string);
        std::string& RemoveSpaces(std::string& string);
        std::string& RemoveCharacter(std::string& string, const char character);

        std::string Demangle(const std::string& string);

        bool IsHiddenFile(const std::string& path);

        TDArray<std::string> SplitString(const std::string& string, const std::string& delimiters);
        TDArray<std::string> SplitString(const std::string& string, const char delimiter);
        TDArray<std::string> Tokenize(const std::string& string);
        TDArray<std::string> GetLines(const std::string& string);

        const char* FindToken(const char* str, const std::string& token);
        const char* FindToken(const std::string& string, const std::string& token);
        int32_t FindStringPosition(const std::string& string, const std::string& search, uint32_t offset = 0);
        std::string StringRange(const std::string& string, uint32_t start, uint32_t length);
        std::string RemoveStringRange(const std::string& string, uint32_t start, uint32_t length);

        std::string GetBlock(const char* str, const char** outPosition = nullptr);
        std::string GetBlock(const std::string& string, uint32_t offset = 0);

        std::string GetStatement(const char* str, const char** outPosition = nullptr);

        bool StringContains(const std::string& string, const std::string& chars);
        bool StartsWith(const std::string& string, const std::string& start);
        int32_t NextInt(const std::string& string);

        bool StringEquals(const std::string& string1, const std::string& string2);
        std::string StringReplace(std::string str, char ch1, char ch2);
        std::string StringReplace(std::string str, char ch);

        std::string BytesToString(uint64_t bytes);

        enum PathType : uint8_t
        {
            Relative,
            DriveAbsolute,
            RootAbsolute,
            Count
        };

        String8 Str8SkipWhitespace(String8 str);
        String8 Str8ChopWhitespace(String8 str);
        String8 Str8SkipChopWhitespace(String8 str);
        String8 Str8SkipChopNewlines(String8 str);

        String8 Str8PathChopLastPeriod(String8 str);
        String8 Str8PathSkipLastSlash(String8 str);
        String8 Str8PathChopLastSlash(String8 str);
        String8 Str8PathSkipLastPeriod(String8 str);
        String8 Str8PathChopPastLastSlash(String8 str);

        PathType PathTypeFromStr8(String8 path);
        String8List PathPartsFromStr8(Arena* arena, String8 path);
        String8List AbsolutePathPartsFromSourcePartsType(Arena* arena, String8 source, String8List parts, PathType type);
        String8 ResolveRelativePath(Arena* arena, String8 path);

        String8List DotResolvedPathPartsFromParts(Arena* arena, String8List parts);
        String8 NormalizedPathFromStr8(Arena* arena, String8 source, String8 path);
        String8 GetFileName(String8 str, bool directory = false);
        String8 AbsolutePathToRelativeFileSystemPath(Arena* arena, String8 path, String8 fileSystemPath, String8 prefix);
        String8 RelativeToAbsolutePath(Arena* arena, String8 path, String8 prefix, String8 fileSystemPath);

        uint64_t BasicHashFromString(String8 string);
        String8 BackSlashesToSlashes(Arena* arena, String8& string);
        String8 SlashesToBackSlashes(Arena* arena, String8& string);
    }
}
