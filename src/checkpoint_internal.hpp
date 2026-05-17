#ifndef _CHECKPOINT_INTERNAL_HPP_
#define _CHECKPOINT_INTERNAL_HPP_

#include "checkpoint_repository.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace checkpoint::internal{
    /* ---------------------------------------------------------
     定数定義
    --------------------------------------------------------- */
    constexpr static std::string kGitMetadataName = ".git";
    constexpr static std::string kGitIgnoreFileName = ".gitignore";
    constexpr static std::string kInitialCheckpointId = "initial";
    constexpr static std::uint32_t kManifestVersion = 1;
    constexpr static std::size_t kHashChunkSize = 8192;



    /* ---------------------------------------------------------
     内部型定義
    --------------------------------------------------------- */
    struct DirectoryRecord{
        std::string relative_path;
        std::uint32_t permissions;
    };

    struct FileRecord{
        std::string relative_path;
        std::uint32_t permissions;
        std::uintmax_t size;
        std::string blob_hash;
    };

    struct SnapshotManifest{
        std::string id;
        std::string parent_id;
        std::string created_at;
        std::vector<DirectoryRecord> directories;
        std::vector<FileRecord> files;
    };

    struct RepositoryLayout{
        std::filesystem::path root_path;
        std::filesystem::path metadata_path;
        std::filesystem::path snapshots_path;
        std::filesystem::path objects_path;
        std::filesystem::path head_path;
    };

    struct GitIgnoreRule{
        std::filesystem::path base_directory_path;
        std::string pattern;
        bool is_negative;
        bool directory_only;
        bool basename_only;
        bool anchored_to_base;
    };

    using DirectoryHandler =
        std::function<void(const std::filesystem::path&, const std::filesystem::file_status&)>;
    using FileHandler =
        std::function<void(const std::filesystem::path&, const std::filesystem::file_status&)>;



    /* ---------------------------------------------------------
     リポジトリ共通処理
    --------------------------------------------------------- */
    std::filesystem::path NormalizePath(const std::filesystem::path& path);

    RepositoryLayout BuildRepositoryLayout(const std::filesystem::path& root_path);

    RepositoryLayout DiscoverRepository(const std::filesystem::path& start_path);

    std::filesystem::path BuildObjectPath(const RepositoryLayout& layout, const std::string& hash);

    std::filesystem::path BuildSnapshotPath(const RepositoryLayout& layout, const std::string& id);

    std::uint32_t StorePermissions(std::filesystem::perms permissions);

    std::filesystem::perms LoadPermissions(std::uint32_t permissions);

    std::string ToGenericString(const std::filesystem::path& path);

    inline std::string DescribePath(const std::filesystem::path& path){
        const auto generic_path = path.generic_string();
        if(generic_path.empty()){
            return ".";
        }

        return generic_path;
    }

    inline std::string BuildRuntimeErrorMessage(const std::string& message){
        return message;
    }

    inline std::string BuildRuntimeErrorMessage(
        const std::string& message,
        const std::string& detail
    ){
        std::ostringstream oss;
        oss << message << ": " << detail;
        return oss.str();
    }

    inline std::string BuildRuntimeErrorMessage(
        const std::string& message,
        const std::filesystem::path& path
    ){
        std::ostringstream oss;
        oss << message << ": path=\"" << DescribePath(path) << '"';
        return oss.str();
    }

    inline std::string BuildRuntimeErrorMessage(
        const std::string& message,
        const std::filesystem::path& path,
        const std::error_code& ec
    ){
        std::ostringstream oss;
        oss << message << ": path=\"" << DescribePath(path) << '"';
        if(ec){
            oss << ", error=\"" << ec.message() << "\", code=" << ec.value();
        }
        return oss.str();
    }

    inline std::string BuildRuntimeErrorMessage(
        const std::string& message,
        const std::filesystem::path& source_path,
        const std::filesystem::path& destination_path,
        const std::error_code& ec
    ){
        std::ostringstream oss;
        oss
            << message
            << ": from=\"" << DescribePath(source_path) << '"'
            << ", to=\"" << DescribePath(destination_path) << '"';
        if(ec){
            oss << ", error=\"" << ec.message() << "\", code=" << ec.value();
        }
        return oss.str();
    }

    void EnsureMetadataDirectories(const RepositoryLayout& layout);

    std::string ReadHead(const RepositoryLayout& layout);

    void WriteHead(const RepositoryLayout& layout, const std::string& id);

    std::string FormatTimestamp();

    bool HasAnySnapshot(const RepositoryLayout& layout);



    /* ---------------------------------------------------------
     ハッシュ計算
    --------------------------------------------------------- */
    std::string ComputeFileHash(const std::filesystem::path& file_path);

    std::string ComputeStringHash(const std::string& text);



    /* ---------------------------------------------------------
     追跡対象判定
    --------------------------------------------------------- */
    bool IsIgnoredEntry(
        const RepositoryLayout& layout,
        const std::filesystem::path& relative_path,
        bool is_directory
    );

    void TraverseTrackedEntries(
        const RepositoryLayout& layout,
        const DirectoryHandler& on_directory,
        const FileHandler& on_file
    );



    /* ---------------------------------------------------------
     スナップショット処理
    --------------------------------------------------------- */
    SnapshotManifest CaptureSnapshot(const RepositoryLayout& layout, const std::string& id);

    void SaveSnapshotManifest(const RepositoryLayout& layout, const SnapshotManifest& manifest);

    SnapshotManifest LoadSnapshotManifest(const std::filesystem::path& snapshot_path);

    SnapshotManifest LoadSnapshotManifest(const RepositoryLayout& layout, const std::string& id);

    void CreateSnapshotInLayout(const RepositoryLayout& layout, const std::string& id);

    void RestoreFiles(const RepositoryLayout& layout, const SnapshotManifest& manifest);
}

#endif
