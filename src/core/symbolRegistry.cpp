// src/core/symbolRegistry.cpp
#include <stdexcept>

#include "symbolRegistry.h"

// string -> ID

SymbolID SymbolRegistry::get_id(const std::string& symbol_str) {
    { // fast check with shared_lock
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = string_to_id_.find(symbol_str);
        if (it != string_to_id_.end()) {
            return it->second;
        }
    } 

    std::unique_lock<std::shared_mutex> lock(mutex_); // take slow exclusive lock if not found
    auto it = string_to_id_.find(symbol_str); // Recheck if other thread added
    if (it != string_to_id_.end()) {
        return it->second;
    }

    SymbolID new_id = nextID_++;
    string_to_id_[symbol_str] = new_id;
    id_to_string_.push_back(symbol_str);
    
    return new_id;
}

// ID -> string

// TODO: For performance-critical code, consider returning std::optional<std::string_view>
//               or const std::string* to avoid throwing an exception on failure.

const std::string& SymbolRegistry::get_string(SymbolID id) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    if (id >= id_to_string_.size()) {
        throw std::out_of_range("SymbolID out of range.");
    }
    return id_to_string_[id];
}
