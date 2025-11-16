#include "trading_engine/symbolRegistry.h"
#include <stdexcept>

SymbolID SymbolRegistry::getID(const std::string& symbol_str) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = string_to_id_.find(symbol_str);
    if (it != string_to_id_.end()) {
        return it->second;
    }

    SymbolID new_id = nextID_++;
    string_to_id_[symbol_str] = new_id;
    id_to_string_.push_back(symbol_str);
    
    return new_id;
}

const std::string& SymbolRegistry::getString(SymbolID id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (id >= id_to_string_.size()) {
        throw std::out_of_range("SymbolID out of range.");
    }
    return id_to_string_[id];
}
