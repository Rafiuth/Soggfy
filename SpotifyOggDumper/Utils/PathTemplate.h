#pragma once
#include <string>
#include <filesystem>
#include <unordered_map>
#include <optional>
#include <regex>
#include "Utils.h"

namespace fs = std::filesystem;

using PathTemplateVars = std::unordered_map<std::string, std::string>;

class PathTemplate
{
public:
    static fs::path Render(const std::string& templt, const PathTemplateVars& vars)
    {
        std::string path = Utils::RegexReplace(templt, std::regex("\\{(.+?)\\}"), [&](auto& m) {
            auto repl = GetReplacement(m.str(1), vars);
            return repl ? *repl : m.str(0);
        });
        return fs::u8path(path);
    }
    static std::optional<std::string> GetReplacement(const std::string& varName, const PathTemplateVars& vars)
    {
        if (varName == "user_home") {
            return Utils::GetHomeDirectory();
        }
        auto itr = vars.find(varName);
        if (itr != vars.end()) {
            return Utils::RemoveInvalidPathChars(itr->second);
        }
        return {};
    }
    //Splits the specified path into directories. e.g.: "A/B/C" -> ["A", "B", "C"]
    static std::vector<std::string> Split(const std::string& str)
    {
        std::vector<std::string> dirs;

        size_t start = 0, end;
        while ((end = str.find_first_of("/\\", start)) != std::string::npos) {
            if (start != end) {
                dirs.push_back(str.substr(start, end - start));
            }
            start = end + 1;
        }
        if (start != str.length()) {
            dirs.push_back(str.substr(start));
        }
        return dirs;
    }
};

struct PathSearchResult
{
    fs::path Path;
    std::vector<std::string> Tokens;
};
using PathSearchList = std::vector<PathSearchResult>;

//Overcomplicated class used to find tracks that have already been downloaded, given limited metadata
class PathTemplateSearcher
{
public:
    PathTemplateSearcher(const std::string& templt) :
        _template(PathTemplate::Split(templt))
    {
    }

    /**
     * @brief Adds a new path on the tree.
     * @param token An arbitrary string that will be associated with the path.
     * @param vars Variables used to render the path.
     * @param unkVars Regex of variables whose value is not known.
    */
    void Add(const std::string& token, const PathTemplateVars& vars, const PathTemplateVars& unkVars)
    {
        Node* node = &_root;
        for (auto& dir : _template) {
            node = node->FindOrAddChild(Directory(dir, vars, unkVars));
        }
        node->Tokens.push_back(token);
    }
    PathSearchList FindExisting()
    {
        PathSearchList list;
        _root.FindExisting(list);
        return list;
    }

private:
    std::vector<std::string> _template;

    struct Directory
    {
        std::string String;
        std::regex Regex;
        bool IsLiteral = true; //true if String is the directory's name; otherwise, Regex must be used to find matches.

        Directory() { }
        Directory(const std::string& str, const PathTemplateVars& vars, const PathTemplateVars& unkVars)
        {
            bool hasUnkVar = false;
            String = Utils::RegexReplace(str, std::regex(R"(\{(.+?)\})"), [&](auto& m) {
                auto varName = m.str(1);
                hasUnkVar |= unkVars.contains(varName);
                
                auto repl = PathTemplate::GetReplacement(varName, vars);
                return repl ? *repl : m.str(0);
            });

            IsLiteral = !hasUnkVar;
            if (hasUnkVar) {
                //escape string literal to build a regex pattern
                static const std::regex specialChars(R"([.^$|()\[\]{}*+?\\])");
                std::string pat = std::regex_replace(String, specialChars, R"(\$&)");
                //fill the placeholders with their respective pattern
                pat = Utils::RegexReplace(pat, std::regex(R"(\\\{(.+?)\\\})"), [&](auto& match) {
                    auto varName = match.str(1);
                    auto itr = unkVars.find(varName);
                    return itr != unkVars.end() ? itr->second : match.str(0);
                });
                Regex = std::regex(pat, std::regex::ECMAScript | std::regex::icase);
            }
        }
    };
    struct Node
    {
        std::vector<std::string> Tokens;
        Directory Dir;
        std::vector<Node> Children;

        Node* FindOrAddChild(const Directory& dir)
        {
            for (auto& child : Children) {
                if (child.Dir.IsLiteral == dir.IsLiteral &&
                    _strcmpi(child.Dir.String.data(), dir.String.data()) == 0)
                {
                    return &child;
                }
            }
            return &Children.emplace_back(Node { .Dir = dir });
        }
        void FindExisting(PathSearchList& results, const fs::path& currPath = {}, bool currPathExists = false)
        {
            if (!currPath.empty() && !(currPathExists || fs::exists(currPath))) return;
            if (Children.empty()) {
                results.push_back({ currPath, Tokens });
                return;
            }
            bool hasRegexChild = false;

            for (auto& child : Children) {
                if (child.Dir.IsLiteral) {
                    child.FindExisting(results, currPath / child.Dir.String);
                } else {
                    hasRegexChild = true;
                }
            }
            if (!hasRegexChild) return;

            for (auto& entry : fs::directory_iterator(currPath)) {
                auto& path = entry.path();
                for (auto& child : Children) {
                    if (std::regex_match(Utils::PathToUtf(path.filename()), child.Dir.Regex)) {
                        child.FindExisting(results, path, true);
                    }
                }
            }
        }
        void Print(std::ostream& out, const std::string& indent)
        {
            out << (Dir.IsLiteral ? "L " : "R ") << Dir.String << '\n';

            for (size_t i = 0; i < Children.size(); i++) {
                bool last = i + 1 == Children.size();

                out << indent << (last ? "'-" : "+-");
                Children[i].Print(out, indent + (last ? "  " : "| "));
            }
        }
    };
    Node _root;
};