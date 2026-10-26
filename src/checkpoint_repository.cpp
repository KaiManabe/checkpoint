#include "checkpoint_internal.hpp"
#include "version.hpp"

#include <charconv>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace checkpoint{

namespace{

void WriteMetadataGitIgnore(const internal::RepositoryLayout& layout){
    static constexpr std::string_view kGitIgnoreContents = "*\n";

    const auto gitignore_path = layout.metadata_path / internal::kGitIgnoreFileName;
    std::ofstream output(gitignore_path, std::ios::trunc);
    if(!output){
        throw std::runtime_error(internal::BuildRuntimeErrorMessage(
            "Failed to create metadata .gitignore",
            gitignore_path
        ));
    }

    output << kGitIgnoreContents;
    if(!output){
        throw std::runtime_error(internal::BuildRuntimeErrorMessage(
            "Failed to write metadata .gitignore",
            gitignore_path
        ));
    }
}

/**
 * @brief 初期化に使用したプログラムのビルドバージョンを保存する
 *
 * @param layout リポジトリ構成
 */
void WriteRepositoryVersion(const internal::RepositoryLayout& layout){
    std::ofstream output(layout.version_path, std::ios::trunc);
    if(!output){
        throw std::runtime_error(internal::BuildRuntimeErrorMessage(
            "Failed to create repository version file",
            layout.version_path
        ));
    }

    output << CHECKPOINT_VERSION_STRING << '\n';
    if(!output){
        throw std::runtime_error(internal::BuildRuntimeErrorMessage(
            "Failed to write repository version",
            layout.version_path
        ));
    }
}

}

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
    WriteMetadataGitIgnore(layout);
    WriteRepositoryVersion(layout);
    internal::InitializeSnapshotHistory(layout);

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
 * @brief @n形式の作成順参照を対応するチェックポイントIDへ解決する
 *
 * @param reference チェックポイントIDまたは相対参照
 * @return std::string 解決後のチェックポイントID
 */
std::string resolve_snapshot_reference(const std::string& reference){
    if(reference.find('@') == std::string::npos){
        return reference;
    }

    if(reference.size() < 2 || reference.front() != '@'){
        throw std::invalid_argument("Invalid relative reference: " + reference);
    }

    std::size_t relative_number = 0;
    const auto number_begin = reference.data() + 1;
    const auto number_end = reference.data() + reference.size();
    const auto parse_result = std::from_chars(
        number_begin,
        number_end,
        relative_number
    );
    if(parse_result.ec != std::errc()
        || parse_result.ptr != number_end
        || relative_number == 0){
        throw std::invalid_argument("Invalid relative reference: " + reference);
    }

    const auto layout = internal::DiscoverRepository(std::filesystem::current_path());
    const auto history = internal::ReadSnapshotHistory(layout);
    if(relative_number > history.size()){
        throw std::invalid_argument("Relative reference is out of range: " + reference);
    }

    const auto& resolved_id = history[history.size() - relative_number];
    internal::LoadSnapshotManifest(layout, resolved_id);
    return resolved_id;
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
