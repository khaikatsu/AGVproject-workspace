#pragma once
#include <string>
#include <algorithm>
inline bool validIdentifier(const std::string& value) {
    return !value.empty() && value.size() <= 128 && std::all_of(value.begin(),value.end(),[](unsigned char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    });
}
