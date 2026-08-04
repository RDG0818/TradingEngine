#pragma once
#include <optional>
#include <string>
#include "core/order.h"

struct ParsedOrderCommand {
    Order order;
    std::string status;          // human-readable confirmation for the status bar
    bool tracks_resting_price;   // true only for a GTC limit order (highlighted in the book)
};

// Parses a "buy"/"sell" order command: qty, optional "@ <price>" (with
// optional trailing "fok"), or optional "stop <price>" / "stop <price>
// @ <limit>". Returns std::nullopt and sets `error` to a usage string
// on bad syntax.
std::optional<ParsedOrderCommand> parse_order_command(const std::string& cmd, TraderId user_id, std::string& error);
