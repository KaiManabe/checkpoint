#include "checkpoint_internal.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace checkpoint::internal{

namespace{

/**
 * @brief 先頭から見て prefix が path に含まれるかを判定する
 *
 * @param path 判定対象のパス
 * @param prefix 接頭辞として期待するパス
 * @return true prefix が一致する場合
 * @return false prefix が一致しない場合
 */
bool HasPathPrefix(const std::filesystem::path& path, const std::filesystem::path& prefix){
    auto path_it = path.begin();
    auto prefix_it = prefix.begin();

    while(prefix_it != prefix.end()){
        if(path_it == path.end() || path_it->generic_string() != prefix_it->generic_string()){
            return false;
        }

        ++path_it;
        ++prefix_it;
    }

    return true;
}


/**
 * @brief path から prefix を除いた残りのパスを返す
 *
 * @param path 元のパス
 * @param prefix 除去する接頭辞
 * @return std::filesystem::path 接頭辞を除いた残りのパス
 */
std::filesystem::path RemovePathPrefix(
    const std::filesystem::path& path,
    const std::filesystem::path& prefix
){
    std::filesystem::path remainder;
    auto path_it = path.begin();
    auto prefix_it = prefix.begin();

    while(prefix_it != prefix.end() && path_it != path.end()){
        ++path_it;
        ++prefix_it;
    }

    while(path_it != path.end()){
        remainder /= *path_it;
        ++path_it;
    }

    return remainder;
}


/**
 * @brief パスのどこかに特定名の要素が含まれるかを判定する
 *
 * @param path 判定対象のパス
 * @param name 探索する要素名
 * @return true 要素が存在する場合
 * @return false 要素が存在しない場合
 */
bool ContainsPathComponent(const std::filesystem::path& path, const std::string& name){
    for(const auto& component : path){
        if(component.generic_string() == name){
            return true;
        }
    }

    return false;
}


/**
 * @brief restore や snapshot の対象外として保護するパスかを判定する
 *
 * @param relative_path ルートからの相対パス
 * @return true 保護対象のパスである場合
 * @return false 通常の追跡対象である場合
 */
bool IsProtectedEntry(const std::filesystem::path& relative_path){
    if(relative_path.empty()){
        return false;
    }

    if(ContainsPathComponent(relative_path, kRepositoryDirectoryName)){
        return true;
    }

    if(ContainsPathComponent(relative_path, kGitMetadataName)){
        return true;
    }

    return false;
}


/**
 * @brief 行末の CR を除去して .gitignore 読み取り結果を正規化する
 *
 * @param text 正規化対象の文字列
 * @return std::string 正規化後の文字列
 */
std::string TrimCarriageReturn(std::string text){
    if(!text.empty() && text.back() == '\r'){
        text.pop_back();
    }

    return text;
}


/**
 * @brief ワイルドカード一致判定をメモ化再帰で実装する
 *
 * @param pattern .gitignore のパターン
 * @param text 判定対象の文字列
 * @param pattern_index pattern の現在位置
 * @param text_index text の現在位置
 * @param memo メモ化領域
 * @return true パターンに一致する場合
 * @return false パターンに一致しない場合
 */
bool MatchWildcardPatternRecursive(
    const std::string& pattern,
    const std::string& text,
    std::size_t pattern_index,
    std::size_t text_index,
    std::vector<std::vector<int>>& memo
){
    auto& cached = memo[pattern_index][text_index];
    if(cached != -1){
        return cached != 0;
    }

    if(pattern_index == pattern.size()){
        cached = (text_index == text.size()) ? 1 : 0;
        return cached != 0;
    }

    if(pattern[pattern_index] == '*'){
        auto next_pattern_index = pattern_index;
        auto matches_separator = false;

        while(next_pattern_index < pattern.size() && pattern[next_pattern_index] == '*'){
            ++next_pattern_index;
        }

        if(next_pattern_index - pattern_index >= 2){
            matches_separator = true;
        }

        if(MatchWildcardPatternRecursive(pattern, text, next_pattern_index, text_index, memo)){
            cached = 1;
            return true;
        }

        for(std::size_t i = text_index; i < text.size(); ++i){
            if(!matches_separator && text[i] == '/'){
                break;
            }

            if(MatchWildcardPatternRecursive(pattern, text, next_pattern_index, i + 1, memo)){
                cached = 1;
                return true;
            }
        }

        cached = 0;
        return false;
    }

    if(pattern[pattern_index] == '?'){
        if(text_index == text.size() || text[text_index] == '/'){
            cached = 0;
            return false;
        }

        cached = MatchWildcardPatternRecursive(
            pattern,
            text,
            pattern_index + 1,
            text_index + 1,
            memo
        ) ? 1 : 0;
        return cached != 0;
    }

    if(text_index == text.size() || pattern[pattern_index] != text[text_index]){
        cached = 0;
        return false;
    }

    cached = MatchWildcardPatternRecursive(
        pattern,
        text,
        pattern_index + 1,
        text_index + 1,
        memo
    ) ? 1 : 0;
    return cached != 0;
}


/**
 * @brief シンプルな * / ? / ** を使ったワイルドカード一致判定を行う
 *
 * @param pattern .gitignore のパターン
 * @param text 判定対象の文字列
 * @return true パターンに一致する場合
 * @return false パターンに一致しない場合
 */
bool MatchWildcardPattern(const std::string& pattern, const std::string& text){
    std::vector<std::vector<int>> memo(
        pattern.size() + 1,
        std::vector<int>(text.size() + 1, -1)
    );

    return MatchWildcardPatternRecursive(pattern, text, 0, 0, memo);
}


/**
 * @brief 指定ディレクトリ直下の .gitignore を読み込んでルール化する
 *
 * @param layout リポジトリ構成
 * @param directory_relative_path .gitignore を探すディレクトリの相対パス
 * @return std::vector<GitIgnoreRule> 読み込んだ ignore ルール群
 */
std::vector<GitIgnoreRule> LoadGitIgnoreRules(
    const RepositoryLayout& layout,
    const std::filesystem::path& directory_relative_path
){
    const auto gitignore_path = layout.root_path / directory_relative_path / kGitIgnoreFileName;
    std::error_code ec;
    if(!std::filesystem::is_regular_file(gitignore_path, ec) || ec){
        return {};
    }

    std::ifstream input(gitignore_path);
    if(!input){
        throw std::runtime_error(BuildRuntimeErrorMessage(
            "Failed to open .gitignore",
            gitignore_path
        ));
    }

    std::vector<GitIgnoreRule> rules;
    std::string line;

    while(std::getline(input, line)){
        auto pattern_text = TrimCarriageReturn(std::move(line));
        if(pattern_text.empty()){
            continue;
        }

        const auto has_escaped_prefix =
            pattern_text.size() >= 2 &&
            pattern_text[0] == '\\' &&
            (pattern_text[1] == '#' || pattern_text[1] == '!');
        if(!has_escaped_prefix && pattern_text[0] == '#'){
            continue;
        }

        if(has_escaped_prefix){
            pattern_text.erase(pattern_text.begin());
        }

        GitIgnoreRule rule;
        rule.base_directory_path = directory_relative_path;
        rule.is_negative = false;
        rule.directory_only = false;
        rule.basename_only = false;
        rule.anchored_to_base = false;

        if(!pattern_text.empty() && pattern_text[0] == '!'){
            rule.is_negative = true;
            pattern_text.erase(pattern_text.begin());
        }

        if(!pattern_text.empty() && pattern_text[0] == '/'){
            rule.anchored_to_base = true;
            pattern_text.erase(pattern_text.begin());
        }

        if(!pattern_text.empty() && pattern_text.back() == '/'){
            rule.directory_only = true;
            while(!pattern_text.empty() && pattern_text.back() == '/'){
                pattern_text.pop_back();
            }
        }

        if(pattern_text.empty()){
            continue;
        }

        rule.basename_only = (pattern_text.find('/') == std::string::npos);
        rule.pattern = pattern_text;
        rules.push_back(rule);
    }

    return rules;
}


/**
 * @brief 1つの ignore ルールが対象パスへ一致するかを判定する
 *
 * @param rule 判定に使うルール
 * @param relative_path_from_base ルール基準ディレクトリからの相対パス
 * @param is_directory 対象がディレクトリかどうか
 * @return true ルールに一致する場合
 * @return false ルールに一致しない場合
 */
bool MatchesGitIgnoreRule(
    const GitIgnoreRule& rule,
    const std::filesystem::path& relative_path_from_base,
    bool is_directory
){
    std::vector<std::string> components;
    components.reserve(static_cast<std::size_t>(
        std::distance(relative_path_from_base.begin(), relative_path_from_base.end())
    ));

    for(const auto& component : relative_path_from_base){
        components.push_back(component.generic_string());
    }

    if(components.empty()){
        return false;
    }

    if(rule.basename_only){
        if(rule.anchored_to_base){
            const auto component_is_directory = (components.size() > 1) || is_directory;
            if(rule.directory_only && !component_is_directory){
                return false;
            }

            return MatchWildcardPattern(rule.pattern, components.front());
        }

        for(std::size_t i = 0; i < components.size(); ++i){
            const auto component_is_directory = (i + 1 < components.size()) || is_directory;
            if(rule.directory_only && !component_is_directory){
                continue;
            }

            if(MatchWildcardPattern(rule.pattern, components[i])){
                return true;
            }
        }

        return false;
    }

    std::filesystem::path prefix_path;
    for(std::size_t i = 0; i < components.size(); ++i){
        prefix_path /= components[i];
        const auto prefix_is_directory = (i + 1 < components.size()) || is_directory;
        if(rule.directory_only && !prefix_is_directory){
            continue;
        }

        if(MatchWildcardPattern(rule.pattern, prefix_path.generic_string())){
            return true;
        }
    }

    return false;
}


/**
 * @brief 読み込み済み .gitignore ルール群に基づいて対象パスが無視されるかを判定する
 *
 * @param relative_path ルートからの相対パス
 * @param is_directory 対象がディレクトリかどうか
 * @param rules 読み込み済みの ignore ルール群
 * @return true ignore 対象である場合
 * @return false ignore 対象でない場合
 */
bool IsIgnoredByGitIgnoreRules(
    const std::filesystem::path& relative_path,
    bool is_directory,
    const std::vector<GitIgnoreRule>& rules
){
    auto ignored = false;

    for(const auto& rule : rules){
        if(!HasPathPrefix(relative_path, rule.base_directory_path)){
            continue;
        }

        const auto relative_path_from_base = RemovePathPrefix(relative_path, rule.base_directory_path);
        if(relative_path_from_base.empty()){
            continue;
        }

        if(MatchesGitIgnoreRule(rule, relative_path_from_base, is_directory)){
            ignored = !rule.is_negative;
        }
    }

    return ignored;
}


/**
 * @brief 指定パスまでに有効な .gitignore ルールを順に読み込む
 *
 * @param layout リポジトリ構成
 * @param relative_path 判定対象の相対パス
 * @return std::vector<GitIgnoreRule> そのパスに適用される ignore ルール群
 */
std::vector<GitIgnoreRule> LoadGitIgnoreRulesForPath(
    const RepositoryLayout& layout,
    const std::filesystem::path& relative_path
){
    std::vector<GitIgnoreRule> rules = LoadGitIgnoreRules(layout, std::filesystem::path());
    std::filesystem::path current_directory_path;
    const auto component_count = static_cast<std::size_t>(
        std::distance(relative_path.begin(), relative_path.end())
    );

    if(component_count == 0){
        return rules;
    }

    std::size_t index = 0;
    for(const auto& component : relative_path){
        if(index + 1 >= component_count){
            break;
        }

        current_directory_path /= component;
        const auto local_rules = LoadGitIgnoreRules(layout, current_directory_path);
        rules.insert(rules.end(), local_rules.begin(), local_rules.end());
        ++index;
    }

    return rules;
}


/**
 * @brief 再帰的に追跡対象を列挙し、コールバックへ通知する
 *
 * @param layout リポジトリ構成
 * @param directory_relative_path 走査中ディレクトリの相対パス
 * @param active_rules 現在有効な ignore ルール群
 * @param on_directory 追跡対象ディレクトリ用コールバック
 * @param on_file 追跡対象ファイル用コールバック
 */
void TraverseTrackedEntriesRecursive(
    const RepositoryLayout& layout,
    const std::filesystem::path& directory_relative_path,
    std::vector<GitIgnoreRule> active_rules,
    const DirectoryHandler& on_directory,
    const FileHandler& on_file
){
    const auto current_directory_path = layout.root_path / directory_relative_path;
    const auto local_rules = LoadGitIgnoreRules(layout, directory_relative_path);
    active_rules.insert(active_rules.end(), local_rules.begin(), local_rules.end());

    std::vector<std::filesystem::directory_entry> entries;
    std::error_code ec;
    for(std::filesystem::directory_iterator it(current_directory_path, ec), end; it != end; it.increment(ec)){
        if(ec){
            throw std::runtime_error(BuildRuntimeErrorMessage(
                "Failed to enumerate repository directory",
                current_directory_path,
                ec
            ));
        }

        entries.push_back(*it);
    }

    std::sort(
        entries.begin(),
        entries.end(),
        [](const auto& lhs, const auto& rhs){
            return lhs.path().filename().generic_string() < rhs.path().filename().generic_string();
        }
    );

    for(const auto& entry : entries){
        const auto relative_path = directory_relative_path / entry.path().filename();
        ec.clear();
        const auto status = entry.symlink_status(ec);
        if(ec){
            throw std::runtime_error(BuildRuntimeErrorMessage(
                "Failed to inspect repository entry",
                layout.root_path / relative_path,
                ec
            ));
        }

        const auto is_directory = std::filesystem::is_directory(status);
        const auto is_regular_file = std::filesystem::is_regular_file(status);

        if(IsProtectedEntry(relative_path)){
            continue;
        }

        if(!is_directory && !is_regular_file){
            throw std::runtime_error(BuildRuntimeErrorMessage(
                "Unsupported file type exists in repository",
                layout.root_path / relative_path
            ));
        }

        if(IsIgnoredByGitIgnoreRules(relative_path, is_directory, active_rules)){
            continue;
        }

        if(is_directory){
            on_directory(relative_path, status);
            TraverseTrackedEntriesRecursive(layout, relative_path, active_rules, on_directory, on_file);
            continue;
        }

        on_file(relative_path, status);
    }
}

}


/**
 * @brief 指定した相対パスが ignore 対象かどうかを判定する
 *
 * @param layout リポジトリ構成
 * @param relative_path ルートからの相対パス
 * @param is_directory 対象がディレクトリかどうか
 * @return true ignore 対象である場合
 * @return false ignore 対象でない場合
 */
bool IsIgnoredEntry(
    const RepositoryLayout& layout,
    const std::filesystem::path& relative_path,
    bool is_directory
){
    if(IsProtectedEntry(relative_path)){
        return true;
    }

    const auto rules = LoadGitIgnoreRulesForPath(layout, relative_path);
    return IsIgnoredByGitIgnoreRules(relative_path, is_directory, rules);
}


/**
 * @brief ignore ルールと保護ルールを適用しながら追跡対象を列挙する
 *
 * @param layout リポジトリ構成
 * @param on_directory 追跡対象ディレクトリ用コールバック
 * @param on_file 追跡対象ファイル用コールバック
 */
void TraverseTrackedEntries(
    const RepositoryLayout& layout,
    const DirectoryHandler& on_directory,
    const FileHandler& on_file
){
    TraverseTrackedEntriesRecursive(layout, std::filesystem::path(), {}, on_directory, on_file);
}

}
