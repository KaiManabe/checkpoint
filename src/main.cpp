#include "arguments.hpp"
#include "checkpoint_repository.hpp"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

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
        << path << " restore my_snapshot_name\n";
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
