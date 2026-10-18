#include "arguments.hpp"
#include "checkpoint_repository.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace{

/**
 * @brief restore 前の退避用 checkpoint を作成する
 *
 * @return std::string 作成した checkpoint ID
 */
std::string CreateRestoreBackupSnapshot(){
    static constexpr int kMaxRetries = 8;

    for(int attempt = 0; attempt < kMaxRetries; ++attempt){
        const auto backup_id = std::string("restore-backup-") + arguments::GenerateCheckpointId();

        try{
            checkpoint::create_snapshot(backup_id);
            return backup_id;
        }catch(const std::invalid_argument& e){
            if(std::string(e.what()) == "Checkpoint id already exists"){
                continue;
            }

            throw;
        }
    }

    throw std::runtime_error(
        "Failed to create restore backup checkpoint after "
        + std::to_string(kMaxRetries)
        + " attempts to generate a unique backup id"
    );
}

/**
 * @brief manifestに保存されたISO 8601時刻を表示用に整形する
 *
 * @param timestamp 保存された時刻
 * @return std::string 表示用時刻
 */
std::string FormatLogTimestamp(std::string timestamp){
    std::tm utc_time{};
    std::istringstream input(timestamp);
    input >> std::get_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");
    if(input.fail()){
        return timestamp;
    }

#ifdef _WIN32
    const auto time = _mkgmtime(&utc_time);
#else
    const auto time = timegm(&utc_time);
#endif
    if(time == static_cast<std::time_t>(-1)){
        return timestamp;
    }

    std::tm local_time{};
#ifdef _WIN32
    if(localtime_s(&local_time, &time) != 0){
        return timestamp;
    }
#else
    if(localtime_r(&time, &local_time) == nullptr){
        return timestamp;
    }
#endif

    std::ostringstream output;
    output << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return output.str();
}

void PrintFileList(
    const std::string& title,
    const std::vector<std::filesystem::path>& files
){
    if(files.empty()){
        return;
    }

    std::cout << title << ": " << files.size() << '\n';
    for(std::size_t i = 0; i < files.size(); ++i){
        std::cout << "    " << i + 1 << ". " << files[i].generic_string() << '\n';
    }
    std::cout << '\n';
}

void PrintLongLog(const std::vector<checkpoint::SnapshotLogEntry>& entries){
    for(std::size_t i = 0; i < entries.size(); ++i){
        const auto& entry = entries[i];
        std::cout
            << "Relative reference : @" << i + 1 << '\n'
            << "Id                 : " << entry.id << '\n'
            << "Date               : " << FormatLogTimestamp(entry.created_at) << "\n\n"
            << "New files      : " << entry.new_files.size() << '\n'
            << "Modified files : " << entry.modified_files.size() << '\n'
            << "Deleted files  : " << entry.deleted_files.size() << "\n\n";

        PrintFileList("New files", entry.new_files);
        PrintFileList("Modified files", entry.modified_files);
        PrintFileList("Deleted files", entry.deleted_files);

        if(i + 1 < entries.size()){
            std::cout << "========================================\n\n";
        }
    }
}

void PrintShortLogLine(
    const std::string& value,
    const std::string& change_label,
    std::size_t change_count,
    std::size_t field_width
){
    std::cout
        << std::left << std::setw(static_cast<int>(field_width)) << value
        << change_label << ": " << change_count << '\n';
}

void PrintShortLog(const std::vector<checkpoint::SnapshotLogEntry>& entries){
    std::size_t field_width = 28;
    for(const auto& entry : entries){
        field_width = std::max(field_width, entry.id.size() + 2);
    }

    for(std::size_t i = 0; i < entries.size(); ++i){
        const auto& entry = entries[i];
        PrintShortLogLine(
            "@" + std::to_string(i + 1),
            "New",
            entry.new_files.size(),
            field_width
        );
        PrintShortLogLine(
            entry.id,
            "Mod",
            entry.modified_files.size(),
            field_width
        );
        PrintShortLogLine(
            FormatLogTimestamp(entry.created_at),
            "Del",
            entry.deleted_files.size(),
            field_width
        );
        std::cout << '\n';
    }
}

}


void PrintUsage(const char* path) {
    std::cerr
        << "\n"
        << "Usage:\n"
        << path << " <mode> <arguments>\n"
        << "\n"
        << "Example:\n"
        << path << " init\n"
        << path << " init ./your_directory\n"
        << path << " snap\n"
        << path << " snap my_snapshot_name\n"
        << path << " restore my_snapshot_name\n"
        << path << " log [-s|--short] [-n count]\n";
}


int main(int argc, char **argv){
    try{
        auto args = arguments::parse_cli_args(argc, argv);
        switch(args.mode){
        case arguments::kInit:{
            auto mode_args = arguments::parse_mode_args<arguments::InitArguments>(args);
            auto repository_root = checkpoint::init_repository(mode_args.root_path);
            std::cout << "Initialized checkpoint repository: " << repository_root << std::endl;
            break;
        }

        case arguments::kCheckpoint:{
            auto mode_args = arguments::parse_mode_args<arguments::CheckpointArguments>(args);
            auto repository_root = checkpoint::create_snapshot(mode_args.id);
            std::cout << mode_args.id << std::endl;
            break;
        }
            
        case arguments::kRestore:{
            auto mode_args = arguments::parse_mode_args<arguments::RestoreArguments>(args);
            const auto backup_id = CreateRestoreBackupSnapshot();
            std::cout << "Backup snapshot: " << backup_id << std::endl;
            auto repository_root = checkpoint::restore_snapshot(mode_args.id);
            std::cout
                << "Restored snap " << mode_args.id
                << " in " << repository_root
                << std::endl;
            break;
        }

        case arguments::kLog:{
            const auto mode_args = arguments::parse_mode_args<arguments::LogArguments>(args);
            const auto entries = checkpoint::get_snapshot_log(mode_args.count);
            if(mode_args.short_format){
                PrintShortLog(entries);
            }else{
                PrintLongLog(entries);
            }
            break;
        }
            
        }
    }catch(const std::invalid_argument& e) {
        std::cerr << e.what() << std::endl;
        PrintUsage(argv[0]);
        return 1;
    }catch(const std::exception &e){
        std::cerr << "Unexpected error:" << std::endl;
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
