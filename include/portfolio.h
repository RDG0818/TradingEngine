// /include/portfolio.h

#include "utils.h"
#include <map>

class Portfolio {

public:

std::map<SymbolID, int> assets;

private:

Price balance_;
TraderID trader_id_;

};