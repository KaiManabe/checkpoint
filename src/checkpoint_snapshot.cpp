#include "checkpoint_internal.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace checkpoint::internal{

namespace{

std::string BuildManifestContext(
    const std::filesystem::path& snapshot_path,
    const std::string& line = std::string()
){
    std::ostringstream oss;
    oss << "path=\"" << DescribePath(snapshot_path) << '"';
    if(!line.empty()){
        oss << ", line=" << std::quoted(line);
    }

    return oss.str();
}

/**
 * @brief 新しいオブジェクトだけを object ストアへ保存する
 *
 * @param layout リポジトリ構成
 * @param source_path 保存元ファイルパス
 * @param hash 保存先を決める内容ハッシュ
 */
void StoreObjectIfNeeded(
    const RepositoryLayout& layout,
    const std::filesystem::path& source_path,
    const std::string& hash
){
    const auto object_path = BuildObjectPath(layout, hash);

    std::error_code ec;
    if(std::filesystem::exists(object_path, ec) && !ec){
        return;
    }

    std::filesystem::create_directories(object_path.parent_path(), ec);
    if(ec){
        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Failed to create object directory",
            object_path.parent_path(),
            ec
        ));
    }

    ec.clear();
    std::filesystem::copy_file(
        source_path,
        object_path,
        std::filesystem::copy_options::skip_existing,
        ec
    );
    if(ec){
        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Failed to store object file",
            source_path,
            object_path,
            ec
        ));
    }
}

}


/**
 * @brief 現在の追跡対象を読み取り、新しい manifest を構築する
 *
 * @param layout リポジトリ構成
 * @param id 作成対象のスナップショットID
 * @return SnapshotManifest 作成された manifest
 */
SnapshotManifest CaptureSnapshot(const RepositoryLayout& layout, const std::string& id){
    SnapshotManifest manifest;
    manifest.id = id;
    manifest.parent_id = ReadHead(layout);
    manifest.created_at = FormatTimestamp();

    TraverseTrackedEntries(
        layout,
        [&manifest](const std::filesystem::path& relative_path, const std::filesystem::file_status& status){
            DirectoryRecord record;
            record.relative_path = ToGenericString(relative_path);
            record.permissions = StorePermissions(status.permissions());
            manifest.directories.push_back(record);
        },
        [&layout, &manifest](const std::filesystem::path& relative_path, const std::filesystem::file_status& status){
            const auto current_path = layout.root_path / relative_path;
            const auto blob_hash = ComputeFileHash(current_path);
            StoreObjectIfNeeded(layout, current_path, blob_hash);

            FileRecord record;
            record.relative_path = ToGenericString(relative_path);
            record.permissions = StorePermissions(status.permissions());
            std::error_code ec;
            record.size = std::filesystem::file_size(current_path, ec);
            if(ec){
                throw std::runtime_error(BuildRuntimeErrorMessage(
                    "Failed to determine file size",
                    current_path,
                    ec
                ));
            }

            record.blob_hash = blob_hash;
            manifest.files.push_back(record);
        }
    );

    std::sort(
        manifest.directories.begin(),
        manifest.directories.end(),
        [](const auto& lhs, const auto& rhs){ return lhs.relative_path < rhs.relative_path; }
    );
    std::sort(
        manifest.files.begin(),
        manifest.files.end(),
        [](const auto& lhs, const auto& rhs){ return lhs.relative_path < rhs.relative_path; }
    );

    return manifest;
}


/**
 * @brief manifest をスナップショットファイルとして保存する
 *
 * @param layout リポジトリ構成
 * @param manifest 保存対象の manifest
 */
void SaveSnapshotManifest(const RepositoryLayout& layout, const SnapshotManifest& manifest){
    const auto snapshot_path = BuildSnapshotPath(layout, manifest.id);
    std::ofstream output(snapshot_path, std::ios::trunc);
    if(!output){
        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Failed to create snapshot manifest",
            snapshot_path
        ));
    }

    output << "version " << kManifestVersion << '\n';
    output << "id " << std::quoted(manifest.id) << '\n';
    output << "parent " << std::quoted(manifest.parent_id) << '\n';
    output << "created_at " << std::quoted(manifest.created_at) << '\n';

    for(const auto& directory : manifest.directories){
        output
            << "D "
            << directory.permissions << ' '
            << std::quoted(directory.relative_path) << '\n';
    }

    for(const auto& file : manifest.files){
        output
            << "F "
            << file.permissions << ' '
            << file.size << ' '
            << file.blob_hash << ' '
            << std::quoted(file.relative_path) << '\n';
    }

    if(!output){
        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Failed to write snapshot manifest",
            snapshot_path
        ));
    }
}


/**
 * @brief スナップショットファイルから manifest を読み込む
 *
 * @param snapshot_path 読み込み対象の manifest パス
 * @return SnapshotManifest 読み込んだ manifest
 */
SnapshotManifest LoadSnapshotManifest(const std::filesystem::path& snapshot_path){
    std::ifstream input(snapshot_path);
    if(!input){
        throw std::invalid_argument("Requested checkpoint id does not exist");
    }

    SnapshotManifest manifest;
    bool version_loaded = false;
    std::string line;

    while(std::getline(input, line)){
        if(line.empty()){
            continue;
        }

        std::istringstream iss(line);
        std::string record_type;
        iss >> record_type;

        if(record_type == "version"){
            std::uint32_t version = 0;
            iss >> version;
            if(iss.fail() || version != kManifestVersion){
                std::ostringstream detail;
                detail
                    << BuildManifestContext(snapshot_path, line)
                    << ", expected_version=" << kManifestVersion
                    << ", actual_version=" << version;
                throw std::runtime_error(BuildRuntimeErrorMessage(
                    "Unsupported snapshot manifest version",
                    detail.str()
                ));
            }

            version_loaded = true;
            continue;
        }

        if(record_type == "id"){
            iss >> std::quoted(manifest.id);
            if(iss.fail()){
                throw std::runtime_error(BuildRuntimeErrorMessage(
                    "Failed to parse snapshot id",
                    BuildManifestContext(snapshot_path, line)
                ));
            }
            continue;
        }

        if(record_type == "parent"){
            iss >> std::quoted(manifest.parent_id);
            if(iss.fail()){
                throw std::runtime_error(BuildRuntimeErrorMessage(
                    "Failed to parse parent id",
                    BuildManifestContext(snapshot_path, line)
                ));
            }
            continue;
        }

        if(record_type == "created_at"){
            iss >> std::quoted(manifest.created_at);
            if(iss.fail()){
                throw std::runtime_error(BuildRuntimeErrorMessage(
                    "Failed to parse timestamp",
                    BuildManifestContext(snapshot_path, line)
                ));
            }
            continue;
        }

        if(record_type == "D"){
            DirectoryRecord record;
            iss >> record.permissions >> std::quoted(record.relative_path);
            if(iss.fail()){
                throw std::runtime_error(BuildRuntimeErrorMessage(
                    "Failed to parse directory record",
                    BuildManifestContext(snapshot_path, line)
                ));
            }

            manifest.directories.push_back(record);
            continue;
        }

        if(record_type == "F"){
            FileRecord record;
            iss >> record.permissions >> record.size >> record.blob_hash >> std::quoted(record.relative_path);
            if(iss.fail()){
                throw std::runtime_error(BuildRuntimeErrorMessage(
                    "Failed to parse file record",
                    BuildManifestContext(snapshot_path, line)
                ));
            }

            manifest.files.push_back(record);
            continue;
        }

        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Unknown record exists in snapshot manifest",
            BuildManifestContext(snapshot_path, line)
        ));
    }

    if(!version_loaded){
        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Snapshot manifest version is missing",
            snapshot_path
        ));
    }

    return manifest;
}


/**
 * @brief スナップショットIDから対応する manifest を読み込む
 *
 * @param layout リポジトリ構成
 * @param id 読み込み対象のスナップショットID
 * @return SnapshotManifest 読み込んだ manifest
 */
SnapshotManifest LoadSnapshotManifest(const RepositoryLayout& layout, const std::string& id){
    const auto snapshot_path = BuildSnapshotPath(layout, id);
    auto manifest = LoadSnapshotManifest(snapshot_path);

    if(manifest.id != id){
        std::ostringstream detail;
        detail
            << "requested_id=" << std::quoted(id)
            << ", actual_id=" << std::quoted(manifest.id)
            << ", path=\"" << DescribePath(snapshot_path) << '"';
        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Snapshot manifest does not match requested checkpoint id",
            detail.str()
        ));
    }

    return manifest;
}


/**
 * @brief 指定レイアウト上へスナップショットを作成し、HEAD を更新する
 *
 * @param layout リポジトリ構成
 * @param id 作成対象のスナップショットID
 */
void CreateSnapshotInLayout(const RepositoryLayout& layout, const std::string& id){
    const auto snapshot_path = BuildSnapshotPath(layout, id);
    if(std::filesystem::exists(snapshot_path)){
        const auto existing_manifest = LoadSnapshotManifest(snapshot_path);
        if(existing_manifest.id == id){
            throw std::invalid_argument("Checkpoint id already exists");
        }

        std::ostringstream detail;
        detail
            << "requested_id=" << std::quoted(id)
            << ", existing_id=" << std::quoted(existing_manifest.id)
            << ", path=\"" << DescribePath(snapshot_path) << '"';
        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Snapshot hash collision detected",
            detail.str()
        ));
    }

    const auto manifest = CaptureSnapshot(layout, id);
    SaveSnapshotManifest(layout, manifest);
    WriteHead(layout, id);
}


/**
 * @brief manifest の内容で現在の作業ディレクトリを復元する
 *
 * @param layout リポジトリ構成
 * @param manifest 復元元の manifest
 */
void RestoreFiles(const RepositoryLayout& layout, const SnapshotManifest& manifest){
    std::vector<std::filesystem::path> current_files;
    std::vector<std::filesystem::path> current_directories;
    std::vector<std::string> desired_files;
    std::vector<std::string> desired_directories;

    desired_directories.reserve(manifest.directories.size());
    for(const auto& directory : manifest.directories){
        if(IsIgnoredEntry(layout, directory.relative_path, true)){
            continue;
        }

        desired_directories.push_back(directory.relative_path);
    }

    desired_files.reserve(manifest.files.size());
    for(const auto& file : manifest.files){
        if(IsIgnoredEntry(layout, file.relative_path, false)){
            continue;
        }

        desired_files.push_back(file.relative_path);
    }

    std::sort(desired_directories.begin(), desired_directories.end());
    std::sort(desired_files.begin(), desired_files.end());

    TraverseTrackedEntries(
        layout,
        [&current_directories](const std::filesystem::path& relative_path, const std::filesystem::file_status&){
            current_directories.push_back(relative_path);
        },
        [&current_files](const std::filesystem::path& relative_path, const std::filesystem::file_status&){
            current_files.push_back(relative_path);
        }
    );

    for(const auto& relative_path : current_files){
        if(std::binary_search(desired_files.begin(), desired_files.end(), ToGenericString(relative_path))){
            continue;
        }

        std::error_code ec;
        std::filesystem::remove(layout.root_path / relative_path, ec);
        if(ec){
            throw std::runtime_error(BuildRuntimeErrorMessage(
                "Failed to remove current file",
                layout.root_path / relative_path,
                ec
            ));
        }
    }

    std::sort(
        current_directories.begin(),
        current_directories.end(),
        [](const auto& lhs, const auto& rhs){
            return std::distance(lhs.begin(), lhs.end()) > std::distance(rhs.begin(), rhs.end());
        }
    );

    for(const auto& relative_path : current_directories){
        if(std::binary_search(desired_directories.begin(), desired_directories.end(), ToGenericString(relative_path))){
            continue;
        }

        std::error_code ec;
        std::filesystem::remove(layout.root_path / relative_path, ec);
        if(ec){
            throw std::runtime_error(BuildRuntimeErrorMessage(
                "Failed to remove current directory",
                layout.root_path / relative_path,
                ec
            ));
        }
    }

    for(const auto& directory : manifest.directories){
        if(IsIgnoredEntry(layout, directory.relative_path, true)){
            continue;
        }

        std::error_code ec;
        const auto directory_path = layout.root_path / std::filesystem::path(directory.relative_path);
        std::filesystem::create_directories(directory_path, ec);
        if(ec){
            throw std::runtime_error(BuildRuntimeErrorMessage(
                "Failed to create directory while restoring",
                directory_path,
                ec
            ));
        }

        ec.clear();
        std::filesystem::permissions(
            directory_path,
            LoadPermissions(directory.permissions),
            std::filesystem::perm_options::replace,
            ec
        );
    }

    for(const auto& file : manifest.files){
        if(IsIgnoredEntry(layout, file.relative_path, false)){
            continue;
        }

        const auto object_path = BuildObjectPath(layout, file.blob_hash);
        if(!std::filesystem::exists(object_path)){
            std::ostringstream detail;
            detail
                << "object_path=\"" << DescribePath(object_path) << '"'
                << ", restore_target=\"" << DescribePath(layout.root_path / std::filesystem::path(file.relative_path)) << '"'
                << ", blob_hash=" << file.blob_hash;
            throw std::runtime_error(BuildRuntimeErrorMessage(
                "Object file required for restore does not exist",
                detail.str()
            ));
        }

        const auto file_path = layout.root_path / std::filesystem::path(file.relative_path);
        std::error_code ec;
        std::filesystem::create_directories(file_path.parent_path(), ec);
        if(ec){
            throw std::runtime_error(BuildRuntimeErrorMessage(
                "Failed to create parent directory while restoring",
                file_path.parent_path(),
                ec
            ));
        }

        ec.clear();
        const auto file_status = std::filesystem::symlink_status(file_path, ec);
        if(ec){
            throw std::runtime_error(BuildRuntimeErrorMessage(
                "Failed to inspect restore target before overwriting",
                file_path,
                ec
            ));
        }

        if(std::filesystem::exists(file_status)
            && (file_status.permissions() & std::filesystem::perms::owner_write)
                == std::filesystem::perms::none){
            std::cout << "Ignoring: " << file.relative_path << std::endl;
            continue;
        }

        ec.clear();
        std::filesystem::copy_file(
            object_path,
            file_path,
            std::filesystem::copy_options::overwrite_existing,
            ec
        );
        if(ec){
            throw std::runtime_error(BuildRuntimeErrorMessage(
                "Failed to restore file from object storage",
                object_path,
                file_path,
                ec
            ));
        }

        ec.clear();
        std::filesystem::permissions(
            file_path,
            LoadPermissions(file.permissions),
            std::filesystem::perm_options::replace,
            ec
        );
    }
}

}
