#include "checkpoint_internal.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace checkpoint{

/**
 * @brief チェックポイント管理用ディレクトリを初期化する
 *
 * @param root_path 管理対象のルートディレクトリ
 * @return std::filesystem::path 初期化したルートディレクトリ
 */
std::filesystem::path init_repository(const std::filesystem::path& root_path){
    std::error_code ec;
    std::filesystem::create_directories(root_path, ec);
    if(ec){
        throw std::runtime_error(internal::BuildRuntimeErrorMessage(
            "Failed to create repository root",
            root_path,
            ec
        ));
    }

    auto layout = internal::BuildRepositoryLayout(root_path);
    internal::EnsureMetadataDirectories(layout);

    if(!std::filesystem::exists(layout.head_path)){
        std::ofstream output(layout.head_path, std::ios::app);
        if(!output){
            throw std::runtime_error(internal::BuildRuntimeErrorMessage(
                "Failed to create HEAD file",
                layout.head_path
            ));
        }
    }

    if(internal::ReadHead(layout).empty() && !internal::HasAnySnapshot(layout)){
        internal::CreateSnapshotInLayout(layout, internal::kInitialCheckpointId);
    }

    return layout.root_path;
}


/**
 * @brief カレントディレクトリから探索した管理対象に対してスナップショットを作成する
 *
 * @param id 作成するチェックポイントID
 * @return std::filesystem::path スナップショットを作成したルートディレクトリ
 */
std::filesystem::path create_snapshot(const std::string& id){
    auto layout = internal::DiscoverRepository(std::filesystem::current_path());
    internal::EnsureMetadataDirectories(layout);
    internal::CreateSnapshotInLayout(layout, id);
    return layout.root_path;
}


/**
 * @brief 指定したチェックポイントIDの内容を作業ディレクトリへ復元する
 *
 * @param id 復元対象のチェックポイントID
 * @return std::filesystem::path 復元を実行したルートディレクトリ
 */
std::filesystem::path restore_snapshot(const std::string& id){
    auto layout = internal::DiscoverRepository(std::filesystem::current_path());
    internal::EnsureMetadataDirectories(layout);

    const auto manifest = internal::LoadSnapshotManifest(layout, id);
    internal::RestoreFiles(layout, manifest);
    internal::WriteHead(layout, id);
    return layout.root_path;
}

}
