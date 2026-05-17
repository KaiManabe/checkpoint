#ifndef _CHECKPOINT_REPOSITORY_HPP_
#define _CHECKPOINT_REPOSITORY_HPP_

#include <filesystem>
#include <string>

namespace checkpoint{
    /* ---------------------------------------------------------
     定数定義
    --------------------------------------------------------- */
    constexpr static std::string kRepositoryDirectoryName = ".checkpoint";
    constexpr static std::string kSnapshotsDirectoryName = "snapshots";
    constexpr static std::string kObjectsDirectoryName = "objects";
    constexpr static std::string kHeadFileName = "HEAD";



    /* ---------------------------------------------------------
     公開関数
    --------------------------------------------------------- */
    std::filesystem::path init_repository(const std::filesystem::path& root_path);

    std::filesystem::path create_snapshot(const std::string& id);

    std::filesystem::path restore_snapshot(const std::string& id);
}

#endif
