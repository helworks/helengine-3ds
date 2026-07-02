#include "platform/3ds/Nintendo3DsRomFsFileSystem.hpp"

#if HELENGINE_NINTENDO_3DS_HAS_GENERATED_CORE

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <vector>

#include "system/io/file-stream.hpp"

namespace helengine::nintendo3ds {
    /// Returns whether one path starts with the Nintendo 3DS RomFS virtual-device prefix.
    bool Nintendo3DsRomFsFileSystem::CanHandlePath(const char* path) {
        if (path == nullptr) {
            return false;
        }

        return std::string(path).rfind("romfs:", 0) == 0;
    }

    /// Returns whether one packaged RomFS path resolves to a readable file.
    bool Nintendo3DsRomFsFileSystem::Exists(const char* path) {
        if (!CanHandlePath(path)) {
            return false;
        }

        const std::string normalizedPath = NormalizePath(path);
        std::FILE* file = std::fopen(normalizedPath.c_str(), "rb");
        if (file == nullptr) {
            return false;
        }

        std::fclose(file);
        return true;
    }

    /// Opens one packaged RomFS file as one read-only memory-backed file stream.
    FileStream* Nintendo3DsRomFsFileSystem::OpenRead(const char* path) {
        if (!CanHandlePath(path)) {
            throw std::invalid_argument("Nintendo 3DS RomFS file-system bridge requires one romfs: path.");
        }

        const std::string normalizedPath = NormalizePath(path);
        std::FILE* file = std::fopen(normalizedPath.c_str(), "rb");
        if (file == nullptr) {
            throw std::runtime_error(std::string("Failed to open file: ") + normalizedPath);
        }

        if (std::fseek(file, 0, SEEK_END) != 0) {
            std::fclose(file);
            throw std::runtime_error(std::string("Failed to seek file: ") + normalizedPath);
        }

        const long fileLength = std::ftell(file);
        if (fileLength < 0) {
            std::fclose(file);
            throw std::runtime_error(std::string("Failed to determine file length: ") + normalizedPath);
        }

        if (std::fseek(file, 0, SEEK_SET) != 0) {
            std::fclose(file);
            throw std::runtime_error(std::string("Failed to seek file: ") + normalizedPath);
        }

        std::vector<uint8_t> bytes(static_cast<std::size_t>(fileLength));
        const std::size_t bytesRead = bytes.empty()
            ? 0U
            : std::fread(bytes.data(), 1U, bytes.size(), file);
        std::fclose(file);
        if (bytesRead != bytes.size()) {
            throw std::runtime_error(std::string("Failed to read file: ") + normalizedPath);
        }

        return new FileStream(bytes.data(), bytes.size());
    }

    /// Normalizes one candidate RomFS path into the slash form expected by libctru stdio hooks.
    std::string Nintendo3DsRomFsFileSystem::NormalizePath(const char* path) {
        if (path == nullptr) {
            throw std::invalid_argument("Nintendo 3DS RomFS path is required.");
        }

        std::string normalized(path);
        std::replace(normalized.begin(), normalized.end(), '\\', '/');
        return normalized;
    }
}

#endif
