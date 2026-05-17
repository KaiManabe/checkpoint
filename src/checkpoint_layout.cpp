#include "checkpoint_internal.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace checkpoint::internal{

/**
 * @brief 与えられたパスを正規化した絶対パスへ変換する
 *
 * @param path 正規化対象のパス
 * @return std::filesystem::path 正規化後の絶対パス
 */
std::filesystem::path NormalizePath(const std::filesystem::path& path){
    std::error_code ec;
    const auto absolute_path = std::filesystem::absolute(path, ec);
    if(ec){
        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Failed to resolve absolute path",
            path,
            ec
        ));
    }

    const auto normalized_path = std::filesystem::weakly_canonical(absolute_path, ec);
    if(ec){
        return absolute_path.lexically_normal();
    }

    return normalized_path;
}


/**
 * @brief ルートディレクトリから管理用パス群を組み立てる
 *
 * @param root_path 管理対象のルートディレクトリ
 * @return RepositoryLayout 管理用ディレクトリ群
 */
RepositoryLayout BuildRepositoryLayout(const std::filesystem::path& root_path){
    RepositoryLayout layout;
    layout.root_path = NormalizePath(root_path);
    layout.metadata_path = layout.root_path / kRepositoryDirectoryName;
    layout.snapshots_path = layout.metadata_path / kSnapshotsDirectoryName;
    layout.objects_path = layout.metadata_path / kObjectsDirectoryName;
    layout.head_path = layout.metadata_path / kHeadFileName;
    return layout;
}


/**
 * @brief 現在位置から親方向へ .checkpoint を探索する
 *
 * @param start_path 探索開始パス
 * @return RepositoryLayout 発見したリポジトリ構成
 */
RepositoryLayout DiscoverRepository(const std::filesystem::path& start_path){
    auto current_path = NormalizePath(start_path);

    while(true){
        auto layout = BuildRepositoryLayout(current_path);
        std::error_code ec;
        if(std::filesystem::is_directory(layout.metadata_path, ec)){
            return layout;
        }

        if(current_path == current_path.root_path()){
            throw std::invalid_argument("Checkpoint repository is not initialized");
        }

        current_path = current_path.parent_path();
    }
}


/**
 * @brief オブジェクト保存パスを内容ハッシュから生成する
 *
 * @param layout リポジトリ構成
 * @param hash オブジェクトの内容ハッシュ
 * @return std::filesystem::path オブジェクト保存先
 */
std::filesystem::path BuildObjectPath(const RepositoryLayout& layout, const std::string& hash){
    return layout.objects_path / hash.substr(0, 2) / hash.substr(2);
}


/**
 * @brief スナップショット manifest の保存パスをIDから生成する
 *
 * @param layout リポジトリ構成
 * @param id スナップショットID
 * @return std::filesystem::path manifest 保存先
 */
std::filesystem::path BuildSnapshotPath(const RepositoryLayout& layout, const std::string& id){
    return layout.snapshots_path / (ComputeStringHash(id) + ".manifest");
}


/**
 * @brief filesystem::perms を永続化しやすい整数値へ変換する
 *
 * @param permissions 変換対象の権限情報
 * @return std::uint32_t 整数化した権限情報
 */
std::uint32_t StorePermissions(std::filesystem::perms permissions){
    return static_cast<std::uint32_t>(permissions);
}


/**
 * @brief 保存済みの整数値から filesystem::perms を復元する
 *
 * @param permissions 復元対象の権限情報
 * @return std::filesystem::perms filesystem用の権限情報
 */
std::filesystem::perms LoadPermissions(std::uint32_t permissions){
    return static_cast<std::filesystem::perms>(permissions);
}


/**
 * @brief パスをスラッシュ区切りの文字列へ変換する
 *
 * @param path 変換対象のパス
 * @return std::string 変換後のパス文字列
 */
std::string ToGenericString(const std::filesystem::path& path){
    return path.generic_string();
}


/**
 * @brief 管理用ディレクトリ群を必要に応じて作成する
 *
 * @param layout リポジトリ構成
 */
void EnsureMetadataDirectories(const RepositoryLayout& layout){
    std::error_code ec;
    std::filesystem::create_directories(layout.snapshots_path, ec);
    if(ec){
        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Failed to create snapshots directory",
            layout.snapshots_path,
            ec
        ));
    }

    std::filesystem::create_directories(layout.objects_path, ec);
    if(ec){
        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Failed to create objects directory",
            layout.objects_path,
            ec
        ));
    }
}


/**
 * @brief HEADファイルから現在のスナップショットIDを取得する
 *
 * @param layout リポジトリ構成
 * @return std::string 現在のスナップショットID
 */
std::string ReadHead(const RepositoryLayout& layout){
    std::ifstream input(layout.head_path);
    if(!input){
        return "";
    }

    std::string head;
    std::getline(input, head);
    return head;
}


/**
 * @brief HEADファイルを指定したスナップショットIDで更新する
 *
 * @param layout リポジトリ構成
 * @param id 更新後のスナップショットID
 */
void WriteHead(const RepositoryLayout& layout, const std::string& id){
    std::ofstream output(layout.head_path, std::ios::trunc);
    if(!output){
        std::ostringstream detail;
        detail
            << "path=\"" << DescribePath(layout.head_path) << '"'
            << ", checkpoint_id=" << std::quoted(id);
        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Failed to update HEAD",
            detail.str()
        ));
    }

    output << id << '\n';
}


/**
 * @brief 現在時刻をUTCのISO 8601文字列へ整形する
 *
 * @return std::string 整形後の時刻文字列
 */
std::string FormatTimestamp(){
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc_time{};

#ifdef _WIN32
    gmtime_s(&utc_time, &time);
#else
    gmtime_r(&time, &utc_time);
#endif

    std::ostringstream oss;
    oss << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}


/**
 * @brief スナップショットが1件でも存在するかを判定する
 *
 * @param layout リポジトリ構成
 * @return true スナップショットが存在する場合
 * @return false スナップショットが存在しない場合
 */
bool HasAnySnapshot(const RepositoryLayout& layout){
    std::error_code ec;
    for(std::filesystem::directory_iterator it(layout.snapshots_path, ec), end; it != end; it.increment(ec)){
        if(ec){
            throw std::runtime_error(BuildRuntimeErrorMessage(
                "Failed to inspect snapshots directory",
                layout.snapshots_path,
                ec
            ));
        }

        return true;
    }

    if(ec){
        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Failed to inspect snapshots directory",
            layout.snapshots_path,
            ec
        ));
    }

    return false;
}

}
