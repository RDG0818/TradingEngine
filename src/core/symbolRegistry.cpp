#include "trading_engine/symbolRegistry.h"
#include <stdexcept>

SymbolRegistry& SymbolRegistry::getInstance() {
    static SymbolRegistry instance;
    return instance;
}

SymbolID SymbolRegistry::registerSymbol(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (symbol_to_id_.count(symbol)) {
        return symbol_to_id_[symbol];
    }
    SymbolID id = next_id_++;
    symbol_to_id_[symbol] = id;
    id_to_symbol_[id] = symbol;
    return id;
}

SymbolID SymbolRegistry::getID(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!symbol_to_id_.count(symbol)) {
        throw std::invalid_argument("Symbol not found: " + symbol);
    }
    return symbol_to_id_[symbol];
}

std::string SymbolRegistry::getSymbol(SymbolID id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!id_to_symbol_.count(id)) {
        throw std::invalid_argument("ID not found: " + std::to_string(id));
    }
    return id_to_symbol_[id];
}