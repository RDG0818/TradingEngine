#include "tui/tui.h"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include "engine/exchange_events.h"
#include "core/order.h"
#include "market/trader.h"

using namespace ftxui;

namespace {

std::string fmt_price(Price p) {
    uint64_t dollars = p / 10000;
    uint64_t cents   = (p % 10000) / 100;
    std::ostringstream ss;
    ss << std::setw(8) << dollars << "." << std::setfill('0') << std::setw(2) << cents;
    return ss.str();
}

std::string fmt_qty(Quantity q) {
    std::ostringstream ss;
    ss << std::setw(6) << q;
    return ss.str();
}

std::string fmt_pnl(int64_t pnl) {
    // pnl in fixed-point (10000 = $1.00)
    int64_t abs_pnl = std::abs(pnl);
    std::ostringstream ss;
    ss << (pnl >= 0 ? "+$" : "-$")
       << abs_pnl / 10000 << "."
       << std::setfill('0') << std::setw(2) << (abs_pnl % 10000) / 100;
    return ss.str();
}

Element depth_bar(Quantity qty, Quantity max_qty, Color bar_color) {
    int filled = max_qty > 0 ? static_cast<int>(qty * 10 / max_qty) : 0;
    filled = std::clamp(filled, 0, 10);
    std::string bar(static_cast<size_t>(filled), '|');
    bar += std::string(static_cast<size_t>(10 - filled), ' ');
    return text(bar) | ftxui::color(bar_color);
}

} // namespace

TUI::TUI(Exchange& exchange, TraderRegistry& registry)
    : exchange_(exchange)
    , registry_(registry)
    , stats_(exchange.event_bus())
{
    user_id_ = exchange_.create_portfolio(10'000'000'000ULL);  // $1M

    book_ = exchange_.book_snapshot();

    tokens_.push_back(exchange_.event_bus().subscribe<BookUpdateEvent>(
        [this](const BookUpdateEvent& e) {
            on_book_update(e.snapshot);
        }));

    tokens_.push_back(exchange_.event_bus().subscribe<FillEvent>(
        [this](const FillEvent& e) {
            {
                std::lock_guard lock(state_mutex_);
                user_bid_prices_.erase(e.fill.fill_price);
                user_ask_prices_.erase(e.fill.fill_price);
            }
            on_fill(e.fill);
        }));

    tokens_.push_back(exchange_.event_bus().subscribe<OrderCancelledEvent>(
        [this](const OrderCancelledEvent& e) {
            (void)e;  // we track by price, not order id
        }));
}

TUI::~TUI() {
    for (auto token : tokens_) exchange_.event_bus().unsubscribe(token);
}

void TUI::on_book_update(const BookSnapshot& snap) {
    std::lock_guard lock(state_mutex_);
    book_ = snap;
}

void TUI::on_fill(const Fill& fill) {
    std::lock_guard lock(state_mutex_);
    recent_fills_.push_front(fill);
    if (recent_fills_.size() > 20) recent_fills_.pop_back();
    if (fill.maker_trader_id == user_id_ || fill.taker_trader_id == user_id_) {
        user_fills_.push_front(fill);
        if (user_fills_.size() > 8) user_fills_.pop_back();
    }
}

static Element render_header_impl(Price last_price, double ops_per_sec) {
    std::string price_str = last_price ? fmt_price(last_price) : "      ---";
    std::string ops_str = std::to_string(static_cast<int>(ops_per_sec)) + "/s";
    return hbox({
        text(" TALAT ") | bold | bgcolor(Color::NavyBlue) | color(Color::CyanLight),
        text("  BTC ") | dim,
        text("$" + price_str) | bold | color(Color::Yellow),
        filler(),
        text("orders: ") | dim,
        text(ops_str + "  ") | color(Color::CyanLight),
    }) | border;
}

void TUI::run() {
    auto screen = ScreenInteractive::Fullscreen();
    screen_ = &screen;

    // Post refresh on book updates
    tokens_.push_back(exchange_.event_bus().subscribe<BookUpdateEvent>(
        [&screen](const BookUpdateEvent&) {
            screen.PostEvent(Event::Custom);
        }));

    auto input_component = Input(&command_input_, "> ");
    auto component = CatchEvent(input_component, [&](Event event) -> bool {
        if (event == Event::Return) {
            handle_command(command_input_);
            command_input_.clear();
            return true;
        }
        if (event == Event::Escape) {
            command_input_.clear();
            return true;
        }
        return false;
    });

    auto renderer = Renderer(component, [&]() -> Element {
        // --- header ---
        Price last_price = 0;
        {
            std::lock_guard lock(state_mutex_);
            if (!recent_fills_.empty()) last_price = recent_fills_.front().fill_price;
        }
        auto stats_snap = stats_.snapshot();
        double ops = stats_snap.orders_per_sec;

        // --- order book ---
        BookSnapshot snap;
        std::unordered_set<Price> my_bids, my_asks;
        {
            std::lock_guard lock(state_mutex_);
            snap = book_;
            my_bids = user_bid_prices_;
            my_asks = user_ask_prices_;
        }

        Elements book_rows;
        book_rows.push_back(text("ORDER BOOK") | bold | center | color(Color::Cyan));
        book_rows.push_back(separator());

        Quantity max_qty = 1;
        for (const auto& [p, q] : snap.asks) max_qty = std::max(max_qty, q);
        for (const auto& [p, q] : snap.bids) max_qty = std::max(max_qty, q);

        // asks (up to 6, lowest nearest mid) — snap.asks is ascending by price
        std::vector<std::pair<Price, Quantity>> asks_display;
        for (const auto& level : snap.asks) {
            asks_display.push_back(level);
            if (asks_display.size() >= 6) break;
        }
        std::reverse(asks_display.begin(), asks_display.end());
        for (const auto& [price, qty] : asks_display) {
            auto row = hbox({
                depth_bar(qty, max_qty, Color::RedLight),
                text("  "),
                text(fmt_price(price)) | color(Color::RedLight),
                text("  "),
                text(fmt_qty(qty)) | color(Color::GrayLight),
            });
            book_rows.push_back(my_asks.count(price) ? row | bgcolor(Color::NavyBlue) : row);
        }
        book_rows.push_back(separator());
        book_rows.push_back(text("     -- mid --") | dim | color(Color::Yellow));
        book_rows.push_back(separator());
        size_t bid_count = 0;
        for (const auto& [price, qty] : snap.bids) {
            if (bid_count++ >= 6) break;
            auto row = hbox({
                depth_bar(qty, max_qty, Color::GreenLight),
                text("  "),
                text(fmt_price(price)) | color(Color::GreenLight),
                text("  "),
                text(fmt_qty(qty)) | color(Color::GrayLight),
            });
            book_rows.push_back(my_bids.count(price) ? row | bgcolor(Color::NavyBlue) : row);
        }

        // --- fills ---
        std::deque<Fill> fills;
        TraderId uid = user_id_;
        {
            std::lock_guard lock(state_mutex_);
            fills = recent_fills_;
        }
        Elements fill_rows;
        fill_rows.push_back(text("RECENT FILLS") | bold | center | color(Color::Cyan));
        fill_rows.push_back(separator());
        for (const auto& f : fills) {
            bool user_fill = (f.taker_trader_id == uid || f.maker_trader_id == uid);
            bool is_buy = (f.taker_side == Side::Buy);
            Color c = is_buy ? Color::GreenLight : Color::RedLight;
            auto row = hbox({
                text(fmt_price(f.fill_price)) | color(c),
                text("  "),
                text(fmt_qty(f.fill_qty)) | color(Color::GrayLight),
                text("  "),
                text(is_buy ? " BUY " : "SELL ") | color(c) | bold,
            });
            fill_rows.push_back(user_fill ? row | bgcolor(Color::NavyBlue) : row);
        }

        // --- stats ---
        std::string status_msg;
        {
            std::lock_guard lock(state_mutex_);
            status_msg = status_message_;
        }
        auto stat_row = [](const std::string& label, const std::string& val) {
            return hbox({
                text(label) | dim | color(Color::GrayLight),
                filler(),
                text(val) | bold | color(Color::Yellow),
            });
        };
        Elements stat_rows;
        stat_rows.push_back(text("STATS") | bold | center | color(Color::Cyan));
        stat_rows.push_back(separator());
        stat_rows.push_back(stat_row("p50 latency", std::to_string(stats_snap.p50_us) + " us"));
        stat_rows.push_back(stat_row("p99 latency", std::to_string(stats_snap.p99_us) + " us"));
        stat_rows.push_back(stat_row("throughput ", std::to_string(static_cast<int>(stats_snap.orders_per_sec)) + "/s"));
        auto book_snap2 = exchange_.book_snapshot();
        stat_rows.push_back(stat_row("book depth ",
            std::to_string(book_snap2.bids.size() + book_snap2.asks.size()) + " lvls"));
        if (uid != 0) {
            auto portfolio = exchange_.portfolio_snapshot(uid);
            int64_t pos   = portfolio.position;
            int64_t upnl  = portfolio.unrealized_pnl;
            int64_t rpnl  = portfolio.realized_pnl;
            int64_t tpnl  = portfolio.total_pnl;
            Price   avg   = portfolio.avg_cost;

            // --- prominent profit indicator ---
            Color tpnl_color = tpnl > 0 ? Color::GreenLight : tpnl < 0 ? Color::RedLight : Color::GrayLight;
            std::string arrow = tpnl > 0 ? " ▲ " : tpnl < 0 ? " ▼ " : " — ";
            stat_rows.push_back(separator());
            stat_rows.push_back(
                hbox({
                    text(arrow) | bold | color(tpnl_color),
                    text(fmt_pnl(tpnl)) | bold | color(tpnl_color),
                    filler(),
                    text("TOTAL PnL") | dim | color(Color::GrayLight),
                }) | bgcolor(tpnl != 0 ? Color::Black : Color::Black)
            );

            // --- breakdown ---
            stat_rows.push_back(separator());
            Color pos_color  = pos  > 0 ? Color::GreenLight : pos  < 0 ? Color::RedLight : Color::GrayLight;
            Color upnl_color = upnl > 0 ? Color::GreenLight : upnl < 0 ? Color::RedLight : Color::GrayLight;
            Color rpnl_color = rpnl > 0 ? Color::GreenLight : rpnl < 0 ? Color::RedLight : Color::GrayLight;
            stat_rows.push_back(hbox({text("position ") | dim | color(Color::GrayLight), filler(),
                text((pos >= 0 ? "+" : "") + std::to_string(pos)) | bold | color(pos_color)}));
            if (avg > 0)
                stat_rows.push_back(hbox({text("avg cost ") | dim | color(Color::GrayLight), filler(),
                    text("$" + std::to_string(avg / 10000)) | bold | color(Color::Yellow)}));
            if (pos != 0)
                stat_rows.push_back(hbox({text("unreal PnL") | dim | color(Color::GrayLight), filler(),
                    text(fmt_pnl(upnl)) | bold | color(upnl_color)}));
            if (rpnl != 0)
                stat_rows.push_back(hbox({text("real PnL ") | dim | color(Color::GrayLight), filler(),
                    text(fmt_pnl(rpnl)) | bold | color(rpnl_color)}));

            // User trade history
            std::deque<Fill> my_fills;
            {
                std::lock_guard lock2(state_mutex_);
                my_fills = user_fills_;
            }
            if (!my_fills.empty()) {
                stat_rows.push_back(separator());
                stat_rows.push_back(text("MY TRADES") | bold | color(Color::Cyan));
                for (const auto& f : my_fills) {
                    Side user_side = (f.taker_trader_id == uid)
                        ? f.taker_side
                        : (f.taker_side == Side::Buy ? Side::Sell : Side::Buy);
                    Color c = (user_side == Side::Buy) ? Color::GreenLight : Color::RedLight;
                    stat_rows.push_back(hbox({
                        text(user_side == Side::Buy ? "BUY " : "SELL") | bold | color(c),
                        text(" "),
                        text(std::to_string(f.fill_qty)) | color(Color::GrayLight),
                        text(" @") | dim,
                        filler(),
                        text(fmt_price(f.fill_price)) | color(Color::Yellow),
                    }));
                }
            }
        }
        if (!status_msg.empty()) {
            stat_rows.push_back(separator());
            // paragraph() doesn't handle \n — split manually
            std::istringstream iss(status_msg);
            std::string line;
            while (std::getline(iss, line)) {
                if (line.empty())
                    stat_rows.push_back(text(" "));
                else
                    stat_rows.push_back(text(line) | color(Color::CyanLight));
            }
        }

        return vbox({
            render_header_impl(last_price, ops),
            hbox({
                vbox(std::move(book_rows)) | border | size(WIDTH, EQUAL, 37),
                vbox(std::move(fill_rows)) | border | size(WIDTH, EQUAL, 32),
                vbox(std::move(stat_rows)) | border | flex,
            }) | flex,
            hbox({
                text(" ❯ ") | color(Color::Cyan) | bold,
                input_component->Render(),
            }) | border | bgcolor(Color::Black),
        });
    });

    screen.Loop(renderer);
    screen_ = nullptr;
}

void TUI::handle_command(const std::string& cmd) {
    if (cmd.empty()) return;
    if (cmd[0] == '/') handle_slash_command(cmd);
    else               handle_order_command(cmd);
}

void TUI::handle_order_command(const std::string& cmd) {
    std::istringstream ss(cmd);
    std::string token;
    ss >> token;

    auto set_status = [this](const std::string& msg) {
        std::lock_guard lock(state_mutex_);
        status_message_ = msg;
    };

    if (token == "q" || token == "quit") {
        if (screen_) screen_->ExitLoopClosure()();
        return;
    }

    if (token == "cancel") {
        OrderId id = 0;
        if (!(ss >> id)) { set_status("Usage: cancel <order_id>"); return; }
        exchange_.cancel_order(id);
        set_status("Cancel requested for #" + std::to_string(id));
        return;
    }

    Side side;
    if (token == "buy")       side = Side::Buy;
    else if (token == "sell") side = Side::Sell;
    else { set_status("Unknown command. Type /help."); return; }

    double qty_d = 0;
    if (!(ss >> qty_d) || qty_d <= 0) {
        set_status("Usage: buy/sell <qty> [@ <price>]");
        return;
    }
    Quantity qty = static_cast<Quantity>(qty_d);
    if (qty == 0) qty = 1;

    std::string at;
    ss >> at;

    if (at == "@") {
        double price_d = 0;
        if (!(ss >> price_d) || price_d <= 0) {
            set_status("Usage: buy <qty> @ <price>"); return;
        }
        Price price = static_cast<Price>(price_d * 10000.0);
        OrderId oid = Trader::alloc_order_id();
        {
            std::lock_guard lock(state_mutex_);
            if (side == Side::Buy) user_bid_prices_.insert(price);
            else                   user_ask_prices_.insert(price);
        }
        exchange_.submit_order(LimitOrder{oid, user_id_, side, price, qty, TimeInForce::GTC, {}});
        set_status((side == Side::Buy ? "Buy " : "Sell ") + std::to_string(qty) +
                   " @ $" + std::to_string(static_cast<int>(price_d)) + "  id=" + std::to_string(oid));
    } else {
        OrderId oid = Trader::alloc_order_id();
        exchange_.submit_order(MarketOrder{oid, user_id_, side, qty, TimeInForce::IOC, {}});
        set_status(std::string("Market ") + (side == Side::Buy ? "buy " : "sell ") +
                   std::to_string(qty) + "  id=" + std::to_string(oid));
    }
}

void TUI::handle_slash_command(const std::string& cmd) {
    std::istringstream ss(cmd.substr(1));
    std::string verb;
    ss >> verb;

    auto set_status = [this](const std::string& msg) {
        std::lock_guard lock(state_mutex_);
        status_message_ = msg;
    };

    if (verb == "help") {
        set_status(
            "ORDERS\n"
            "  buy <qty> @ <price>   limit buy (GTC)\n"
            "  sell <qty> @ <price>  limit sell (GTC)\n"
            "  buy <qty>             market buy (IOC)\n"
            "  sell <qty>            market sell (IOC)\n"
            "  cancel <id>           cancel resting order\n"
            "  q / quit              exit\n"
            "\n"
            "MARKET CONTROLS  (defaults)\n"
            "  /vol <0.0-1.0>        GBM volatility sigma (0.0003)\n"
            "  /spread <dollars>     MM half-spread in $ (2.00)\n"
            "  /speed <1-10>         tick speed, 1=2s 10=200ms (1)\n"
            "  /pause / /resume      halt/restart all traders\n"
            "  /traders <mm|informed|noise> <n>\n"
            "                        set trader count (1/2/3)"
        );
        return;
    }
    if (verb == "vol") {
        double v = 0;
        if (!(ss >> v) || v < 0 || v > 1.0) { set_status("Usage: /vol <0.0-1.0>"); return; }
        registry_.set_sigma(v);
        set_status("Volatility set to " + std::to_string(v));
        return;
    }
    if (verb == "spread") {
        double dollars = 0;
        if (!(ss >> dollars) || dollars <= 0) { set_status("Usage: /spread <dollars>"); return; }
        Price half_spread = static_cast<Price>(dollars * 10000.0 / 2.0);
        registry_.set_market_maker_spread(half_spread);
        set_status("MM half-spread set to $" + std::to_string(dollars / 2.0));
        return;
    }
    if (verb == "speed") {
        int speed = 0;
        if (!(ss >> speed) || speed < 1 || speed > 10) { set_status("Usage: /speed <1-10>"); return; }
        registry_.set_tick_interval_ms(2000 / speed);
        set_status("Tick interval " + std::to_string(2000 / speed) + "ms");
        return;
    }
    if (verb == "pause") {
        registry_.pause_all();
        set_status("Market paused. /resume to continue.");
        return;
    }
    if (verb == "resume") {
        registry_.resume_all();
        set_status("Market resumed.");
        return;
    }
    if (verb == "traders") {
        std::string type; int n = 0;
        if (!(ss >> type >> n) || n < 0) { set_status("Usage: /traders <mm|informed|noise> <n>"); return; }
        if      (type == "mm")       registry_.set_market_maker_count(static_cast<size_t>(n));
        else if (type == "informed") registry_.set_informed_count(static_cast<size_t>(n));
        else if (type == "noise")    registry_.set_noise_count(static_cast<size_t>(n));
        else { set_status("Unknown type: mm, informed, noise"); return; }
        set_status("Set " + type + " count to " + std::to_string(n));
        return;
    }
    set_status("Unknown: '" + verb + "'. Type /help.");
}
