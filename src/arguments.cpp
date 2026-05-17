#include "arguments.hpp"

#include <array>
#include <random>
#include <sstream>
#include <stdexcept>


namespace arguments{

/**
 * @brief デフォルトで利用するチェックポイントIDを生成する
 *
 * @return std::string 16進数ベースのランダムなID
 */
std::string GenerateCheckpointId(){
    static thread_local std::mt19937_64 engine(std::random_device{}());
    static constexpr char kHex[] = "0123456789abcdef";
    static constexpr std::array<int, 5> kGroupSizes = {8, 4, 4, 4, 12};

    std::uniform_int_distribution<int> dist(0, 15);
    std::string id;

    for(std::size_t group_index = 0; group_index < kGroupSizes.size(); ++group_index){
        if(group_index != 0){
            id.push_back('-');
        }

        for(int i = 0; i < kGroupSizes[group_index]; ++i){
            id.push_back(kHex[dist(engine)]);
        }
    }

    return id;
}


/**
    * @brief コマンドライン引数を受けとり，Arguments構造体に変換する
    * 
    * @param argv main関数から渡されるargv
    * @param argc main関数から渡されるargc
    * @return Arguments 構造体に変換された引数
    */
Arguments parse_cli_args(int argv, char **argc){
    // ----------------------- 引数チェック -----------------------
    if(argv < 2){
        std::string msg = "Missing required argument: mode";
        throw std::invalid_argument(msg);
    }

    // ------------------------ モード判定 ------------------------
    auto mode = std::string(argc[1]);
    Arguments args;

    if(mode == "init"){
        args.mode = kInit;
    }else if(mode == "snap"){
        args.mode = kCheckpoint;
    }else if(mode == "restore"){
        args.mode = kRestore;
    }else{
        std::ostringstream oss;
        oss << "Unknown argument: mode (" << mode << ")";
        std::string msg = oss.str();
        throw std::invalid_argument(msg);
    }

    // ------------------------- 引数格納 -------------------------
    args.args.resize(argv - 2);
    for(int i = 2; i < argv; ++i){
        args.args[i - 2] = std::string(argc[i]);
    }

    return args;
}


/**
 * @brief argcから生成されたArguments構造体をkInitモードの引数として解釈する
 * 
 * @tparam  
 * @param args Arguments構造体
 * @return InitArgments 解釈された引数
 */
template <>
InitArguments parse_mode_args<InitArguments>(Arguments args){
    InitArguments ret;

    if(args.args.size() > 1){
        throw std::invalid_argument("Too many arguments for init mode");
    }
    
    // ---------------- 未指定ならデフォルト値を使用 ----------------
    if(args.args.size() == 0){
        ret.root_path = std::filesystem::path(kInitModeRootPath);
        return ret;
    }


    // ---------------- ルートディレクトリパスを取得 ----------------
    ret.root_path = std::filesystem::path(args.args[0]);

    return ret;
}


/**
 * @brief argcから生成されたArguments構造体をkCheckpointモードの引数として解釈する
 * 
 * @param args Arguments構造体
 * @return CheckpointArgments 解釈された引数
 */
template <>
CheckpointArguments parse_mode_args<CheckpointArguments>(Arguments args){
    CheckpointArguments ret;

    if(args.args.size() > 1){
        throw std::invalid_argument("Too many arguments for snap mode");
    }

    // ---------------- 未指定ならデフォルト値を使用 ----------------
    if(args.args.size() == 0){
        ret.id = GenerateCheckpointId();
        return ret;
    }

    // ----------- チェックポイントIDが明示されていれば指定 -----------
    ret.id = args.args[0];
    return ret;
}


/**
 * @brief argcから生成されたArguments構造体をkRestoreモードの引数として解釈する
 * 
 * @param args Arguments構造体
 * @return RestoreArgments 解釈された引数
 */
template <>
RestoreArguments parse_mode_args<RestoreArguments>(Arguments args){
    RestoreArguments ret;

    if(args.args.size() > 1){
        throw std::invalid_argument("Too many arguments for restore mode");
    }

    // ---------------- 未指定なら例外を投げる ----------------
    if(args.args.size() == 0){
        throw std::invalid_argument("Missing required argument: id");
    }

    // ----------- チェックポイントIDを指定 -----------
    ret.id = args.args[0];
    return ret;
}



}
