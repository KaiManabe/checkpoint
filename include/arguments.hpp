#ifndef _ARGUMENTS_HPP_
#define _ARGUMENTS_HPP_

#include <cstddef>
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
        kLog,
        kVersion,
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

    struct LogArguments{
        bool short_format = false;
        std::size_t count = 5;
    };

    struct VersionArguments{};

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

    template <>
    LogArguments parse_mode_args<LogArguments>(Arguments args);

    template <>
    VersionArguments parse_mode_args<VersionArguments>(Arguments args);

}

#endif
