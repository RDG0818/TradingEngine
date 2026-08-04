#include "tui/order_command_parser.h"
#include "market/trader.h"
#include <sstream>

std::optional<ParsedOrderCommand> parse_order_command(const std::string& cmd, TraderId user_id, std::string& error) {
    std::istringstream ss(cmd);
    std::string token;
    ss >> token;

    Side side;
    if (token == "buy")       side = Side::Buy;
    else if (token == "sell") side = Side::Sell;
    else { error = "Unknown command. Type /help."; return std::nullopt; }

    double qty_d = 0;
    if (!(ss >> qty_d) || qty_d <= 0) {
        error = "Usage: buy/sell <qty> [@ <price> [fok]] [stop <price> [@ <limit>]]";
        return std::nullopt;
    }
    Quantity qty = static_cast<Quantity>(qty_d);
    if (qty == 0) qty = 1;

    std::string modifier;
    ss >> modifier;

    OrderId oid = Trader::alloc_order_id();

    if (modifier == "@") {
        double price_d = 0;
        if (!(ss >> price_d) || price_d <= 0) {
            error = "Usage: buy <qty> @ <price> [fok]";
            return std::nullopt;
        }
        Price price = static_cast<Price>(price_d * 10000.0);

        std::string trailing;
        ss >> trailing;
        TimeInForce tif = TimeInForce::GTC;
        if (trailing == "fok") tif = TimeInForce::FOK;
        else if (!trailing.empty()) {
            error = "Usage: buy <qty> @ <price> [fok]";
            return std::nullopt;
        }

        ParsedOrderCommand result;
        result.order = LimitOrder{oid, user_id, side, price, qty, tif, {}};
        result.tracks_resting_price = (tif == TimeInForce::GTC);
        result.status = (side == Side::Buy ? "Buy " : "Sell ") + std::to_string(qty) +
                         " @ $" + std::to_string(static_cast<int>(price_d)) +
                         (tif == TimeInForce::FOK ? " FOK" : "") +
                         "  id=" + std::to_string(oid);
        return result;
    }

    if (modifier == "stop") {
        double stop_d = 0;
        if (!(ss >> stop_d) || stop_d <= 0) {
            error = "Usage: buy <qty> stop <stop_price> [@ <limit_price>]";
            return std::nullopt;
        }
        Price stop_price = static_cast<Price>(stop_d * 10000.0);

        std::string at;
        ss >> at;

        ParsedOrderCommand result;
        result.tracks_resting_price = false;

        if (at == "@") {
            double limit_d = 0;
            if (!(ss >> limit_d) || limit_d <= 0) {
                error = "Usage: buy <qty> stop <stop_price> @ <limit_price>";
                return std::nullopt;
            }
            Price limit_price = static_cast<Price>(limit_d * 10000.0);
            result.order = StopLimitOrder{oid, user_id, side, stop_price, limit_price, qty, {}};
            result.status = std::string(side == Side::Buy ? "Buy " : "Sell ") + std::to_string(qty) +
                             " stop $" + std::to_string(static_cast<int>(stop_d)) +
                             " limit $" + std::to_string(static_cast<int>(limit_d)) +
                             "  id=" + std::to_string(oid);
        } else if (at.empty()) {
            result.order = StopMarketOrder{oid, user_id, side, stop_price, qty, {}};
            result.status = std::string(side == Side::Buy ? "Buy " : "Sell ") + std::to_string(qty) +
                             " stop $" + std::to_string(static_cast<int>(stop_d)) +
                             "  id=" + std::to_string(oid);
        } else {
            error = "Usage: buy <qty> stop <stop_price> [@ <limit_price>]";
            return std::nullopt;
        }
        return result;
    }

    if (!modifier.empty()) {
        error = "Unknown order modifier '" + modifier + "'. Type /help.";
        return std::nullopt;
    }

    ParsedOrderCommand result;
    result.order = MarketOrder{oid, user_id, side, qty, TimeInForce::IOC, {}};
    result.tracks_resting_price = false;
    result.status = std::string("Market ") + (side == Side::Buy ? "buy " : "sell ") +
                     std::to_string(qty) + "  id=" + std::to_string(oid);
    return result;
}
