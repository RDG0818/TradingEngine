#pragma once

#include "utils.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

// For mapping ticker symbols to integer IDs

class SymbolRegistry {
public:
    // Meyer's Singleton 
    SymbolRegistry(const SymbolRegistry&) = delete;
    SymbolRegistry& operator=(const SymbolRegistry&) = delete;
    static SymbolRegistry& getInstance() { 
        static SymbolRegistry instance; 
        return instance;
    }

    SymbolID getID(const std::string& symbol_str);
    const std::string& getString(SymbolID id) const;

private:
    SymbolRegistry() = default;

    SymbolID nextID_ = 0;
    std::unordered_map<std::string, SymbolID> string_to_id_;
    std::vector<std::string> id_to_string_;
    mutable std::mutex mutex_; 
};
