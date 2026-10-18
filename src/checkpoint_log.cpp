#include "checkpoint_internal.hpp"

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace checkpoint{

namespace{

/**
 * @brief スナップショットとその親のファイル差分をログ項目へ格納する
 *
 * @param layout リポジトリ構成
 * @param current 対象スナップショット
 * @param parent 親スナップショット。親がない場合は空のmanifest
 * @return SnapshotLogEntry 作成したログ項目
 */
SnapshotLogEntry BuildLogEntry(
    const internal::RepositoryLayout& layout,
    const internal::SnapshotManifest& current,
    const internal::SnapshotManifest& parent
){
    SnapshotLogEntry entry;
    entry.id = current.id;
    entry.created_at = current.created_at;

    std::map<std::string, const internal::FileRecord*> current_files;
    std::map<std::string, const internal::FileRecord*> parent_files;

    for(const auto& file : current.files){
        current_files[file.relative_path] = &file;
    }
    for(const auto& file : parent.files){
        parent_files[file.relative_path] = &file;
    }

    for(const auto& [relative_path, current_file] : current_files){
        const auto parent_it = parent_files.find(relative_path);
        const auto absolute_path = layout.root_path / std::filesystem::path(relative_path);

        if(parent_it == parent_files.end()){
            entry.new_files.push_back(absolute_path);
            continue;
        }

        const auto* parent_file = parent_it->second;
        if(current_file->blob_hash != parent_file->blob_hash
            || current_file->permissions != parent_file->permissions){
            entry.modified_files.push_back(absolute_path);
        }
    }

    for(const auto& [relative_path, parent_file] : parent_files){
        (void)parent_file;
        if(current_files.find(relative_path) == current_files.end()){
            entry.deleted_files.push_back(
                layout.root_path / std::filesystem::path(relative_path)
            );
        }
    }

    return entry;
}

}


/**
 * @brief 作成履歴から新しい順にスナップショットログを取得する
 *
 * @param max_count 取得する最大件数
 * @return std::vector<SnapshotLogEntry> 新しい順のログ項目
 */
std::vector<SnapshotLogEntry> get_snapshot_log(std::size_t max_count){
    const auto layout = internal::DiscoverRepository(std::filesystem::current_path());
    std::vector<SnapshotLogEntry> entries;
    const auto history = internal::ReadSnapshotHistory(layout);
    const auto entry_count = std::min(max_count, history.size());

    for(std::size_t i = 0; i < entry_count; ++i){
        const auto& current_id = history[history.size() - 1 - i];
        const auto current = internal::LoadSnapshotManifest(layout, current_id);
        internal::SnapshotManifest parent;
        if(!current.parent_id.empty()){
            parent = internal::LoadSnapshotManifest(layout, current.parent_id);
        }

        entries.push_back(BuildLogEntry(layout, current, parent));
    }

    return entries;
}

}
