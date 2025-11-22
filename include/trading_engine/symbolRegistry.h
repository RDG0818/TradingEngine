#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include "types.h"

class SymbolRegistry {
public:
    static SymbolRegistry& getInstance();

    SymbolID registerSymbol(const std::string& symbol);
    SymbolID getID(const std::string& symbol);
    std::string getSymbol(SymbolID id);

private:
    SymbolRegistry() = default;
    ~SymbolRegistry() = default;
    SymbolRegistry(const SymbolRegistry&) = delete;
    SymbolRegistry& operator=(const SymbolRegistry&) = delete;

    std::mutex mutex_;
    std::unordered_map<std::string, SymbolID> symbol_to_id_;
    std::unordered_map<SymbolID, std::string> id_to_symbol_;
    SymbolID next_id_ = 0;
};