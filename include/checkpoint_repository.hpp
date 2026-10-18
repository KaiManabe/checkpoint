#ifndef _CHECKPOINT_REPOSITORY_HPP_
#define _CHECKPOINT_REPOSITORY_HPP_

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace checkpoint{
    /* ---------------------------------------------------------
     定数定義
    --------------------------------------------------------- */
    constexpr static std::string kRepositoryDirectoryName = ".checkpoint";
    constexpr static std::string kSnapshotsDirectoryName = "snapshots";
    constexpr static std::string kObjectsDirectoryName = "objects";
    constexpr static std::string kHeadFileName = "HEAD";



    /* ---------------------------------------------------------
     型定義
    --------------------------------------------------------- */
    struct SnapshotLogEntry{
        std::string id;
        std::string created_at;
        std::vector<std::filesystem::path> new_files;
        std::vector<std::filesystem::path> modified_files;
        std::vector<std::filesystem::path> deleted_files;
    };



    /* ---------------------------------------------------------
     公開関数
    --------------------------------------------------------- */
    std::filesystem::path init_repository(const std::filesystem::path& root_path);

    std::filesystem::path create_snapshot(const std::string& id);

    std::filesystem::path restore_snapshot(const std::string& id);

    std::vector<SnapshotLogEntry> get_snapshot_log(std::size_t max_count);
}

#endif
