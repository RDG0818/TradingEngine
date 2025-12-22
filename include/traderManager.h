// include/traderManager.h

#include "trader.h"

// class to run all Traders tick function on a single thread

class TraderManager {

public:

  TraderManager(std::chrono::milliseconds tick_interval) : tick_interval(tick_interval) {};
  ~TraderManager() {stop();};

  void addTrader(std::unique_ptr<Trader> trader) {
    traders.push_back(std::move(trader));
  };

  void start() {
    running = true;
    thread = std::thread(&TraderManager::run, this);
  };

  void stop() {
    running = false;
    if (thread.joinable()) {
      thread.join();
    }
  };

private:

  std::vector<std::unique_ptr<Trader>> traders;
  std::chrono::milliseconds tick_interval;
  std::atomic<bool> running{false};
  std::thread thread;

  void run() {
    while (running) {
      for (auto& trader : traders) {
        trader->tick();
      }
      std::this_thread::sleep_for(tick_interval);
    }
  };

};