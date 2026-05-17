#ifndef _ARGUMENTS_HPP_
#define _ARGUMENTS_HPP_

#include <string>
#include <filesystem>
#include <vector>

namespace arguments{
    /* ---------------------------------------------------------
     定数定義
    --------------------------------------------------------- */
    constexpr static std::string kInitModeRootPath = ".";



    /* ---------------------------------------------------------
     型定義
    --------------------------------------------------------- */
    enum Mode{
        kInit,
        kCheckpoint,
        kRestore,
    };

    struct Arguments{
        Mode mode;
        std::vector<std::string> args;
    };

    struct InitArguments{
        std::filesystem::path root_path;
    };

    struct CheckpointArguments{
        std::string id;
    };

    struct RestoreArguments{
        std::string id;
    };

    /* ---------------------------------------------------------
     ヘルパー関数
    --------------------------------------------------------- */
    std::string GenerateCheckpointId();

    Arguments parse_cli_args(int argv, char **argc);

    template <typename T>
    T parse_mode_args(Arguments args);

    template <>
    InitArguments parse_mode_args<InitArguments>(Arguments args);

    template <>
    CheckpointArguments parse_mode_args<CheckpointArguments>(Arguments args);

    template <>
    RestoreArguments parse_mode_args<RestoreArguments>(Arguments args);

}

#endif
