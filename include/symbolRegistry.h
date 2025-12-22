// include/symbolRegistry.h

#ifndef TRADINGENGINE_INCLUDE_SYMBOLREGISTRY_H_
#define TRADINGENGINE_INCLUDE_SYMBOLREGISTRY_H_

#include <shared_mutex>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "utils.h"

// TODO: Consider replacing the Singleton pattern with Dependency Injection.
//               Create the registry instance in main() and pass it to the components
//               that need it to improve testability and make dependencies explicit.
class SymbolRegistry {
public:
    // Meyer's Singleton 
    SymbolRegistry(const SymbolRegistry&) = delete;
    SymbolRegistry& operator=(const SymbolRegistry&) = delete;
    static SymbolRegistry& get_instance() { 
        static SymbolRegistry instance; 
        return instance;
    }

    SymbolID get_id(const std::string& symbol_str);
    const std::string& get_string(SymbolID id) const;

private:
    SymbolRegistry() = default;

    SymbolID nextID_ = 0;
    std::unordered_map<std::string, SymbolID> string_to_id_;
    std::vector<std::string> id_to_string_;
    mutable std::shared_mutex mutex_; 
};

#endif // TRADINGENGINE_INCLUDE_SYMBOLREGISTRY_H_
