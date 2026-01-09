// tests/cpp/traderManagerTest.cpp

#include "gtest/gtest.h"
#include "traderManager.h"
#include "matchingEngine.h"
#include "eventDispatcher.h"
#include "utils.h"
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <unordered_map>

class TraderManagerTest : public ::testing::Test {
protected:
    EventDispatcher dispatcher;
    OrderIdGenerator id_generator;
    MatchingEngine engine;
    std::vector<std::string> symbols = {"AAPL", "GOOG"};
    std::unique_ptr<TraderManager> manager;

    TraderManagerTest() : engine(dispatcher, id_generator) {}

    void SetUp() override {
        manager = std::make_unique<TraderManager>(engine, dispatcher, std::chrono::milliseconds(100), symbols);
    }
};

TEST_F(TraderManagerTest, AddTraderWithUniqueName) {
    bool success = manager->addRandomMarketTrader("rmt_1", 1.0f, std::chrono::milliseconds(100), 10);
    ASSERT_TRUE(success);
}

TEST_F(TraderManagerTest, AddTraderWithDuplicateName) {
    manager->addRandomMarketTrader("rmt_1", 1.0f, std::chrono::milliseconds(100), 10);
    bool success = manager->addRandomMarketTrader("rmt_1", 2.0f, std::chrono::milliseconds(100), 20);
    ASSERT_FALSE(success);
}

TEST_F(TraderManagerTest, RemoveExistingTrader) {
    manager->addRandomMarketTrader("rmt_to_remove", 1.0f, std::chrono::milliseconds(100), 10);
    bool removed = manager->removeTrader("rmt_to_remove");
    ASSERT_TRUE(removed);

    // Verify it's gone
    auto params = manager->getTraderParameters("rmt_to_remove");
    ASSERT_FALSE(params.has_value());
}

TEST_F(TraderManagerTest, RemoveNonExistentTrader) {
    bool removed = manager->removeTrader("non_existent_trader");
    ASSERT_FALSE(removed);
}

TEST_F(TraderManagerTest, GetAndSetRandomMarketTraderParams) {
    manager->addRandomMarketTrader("rmt_1", 5.0f, std::chrono::milliseconds(100), 10);

    auto initial_params = manager->getTraderParameters("rmt_1");
    ASSERT_TRUE(initial_params.has_value());
    ASSERT_EQ(initial_params->count("lambda"), 1);
    EXPECT_EQ(initial_params->at("lambda"), 5.0);

    bool set_success = manager->setTraderParameters("rmt_1", {{"lambda", 10.0}});
    ASSERT_TRUE(set_success);

    auto updated_params = manager->getTraderParameters("rmt_1");
    ASSERT_TRUE(updated_params.has_value());
    EXPECT_EQ(updated_params->at("lambda"), 10.0);
}

TEST_F(TraderManagerTest, GetAndSetRandomLimitTraderParams) {
    manager->addRandomLimitTrader("rlt_1", 3.0f, std::chrono::milliseconds(100), 5, 0.5f);

    auto initial_params = manager->getTraderParameters("rlt_1");
    ASSERT_TRUE(initial_params.has_value());
    ASSERT_EQ(initial_params->count("lambda"), 1);
    EXPECT_EQ(initial_params->at("lambda"), 3.0);
    ASSERT_EQ(initial_params->count("norm_dist_var"), 1);
    EXPECT_EQ(initial_params->at("norm_dist_var"), 0.5);

    bool set_success = manager->setTraderParameters("rlt_1", {{"lambda", 6.0}, {"norm_dist_var", 0.75}});
    ASSERT_TRUE(set_success);

    auto updated_params = manager->getTraderParameters("rlt_1");
    ASSERT_TRUE(updated_params.has_value());
    EXPECT_EQ(updated_params->at("lambda"), 6.0);
    EXPECT_EQ(updated_params->at("norm_dist_var"), 0.75);
}

TEST_F(TraderManagerTest, GetAndSetMarketMakerTraderParams) {
    std::unordered_map<std::string, double> init_prices = {{"AAPL", 150.0}};
    manager->addMarketMakerTrader("mmt_1", 0.1, 0.2, 0.01, std::chrono::milliseconds(100), 10, init_prices);

    auto initial_params = manager->getTraderParameters("mmt_1");
    ASSERT_TRUE(initial_params.has_value());
    EXPECT_EQ(initial_params->at("mu"), 0.1);
    EXPECT_EQ(initial_params->at("sigma"), 0.2);
    EXPECT_EQ(initial_params->at("spread"), 0.01);

    bool set_success = manager->setTraderParameters("mmt_1", {{"mu", 0.5}, {"sigma", 0.6}, {"spread", 0.05}});
    ASSERT_TRUE(set_success);

    auto updated_params = manager->getTraderParameters("mmt_1");
    ASSERT_TRUE(updated_params.has_value());
    EXPECT_EQ(updated_params->at("mu"), 0.5);
    EXPECT_EQ(updated_params->at("sigma"), 0.6);
    EXPECT_EQ(updated_params->at("spread"), 0.05);
}

TEST_F(TraderManagerTest, SetPartialParameters) {
    manager->addRandomLimitTrader("rlt_1", 3.0f, std::chrono::milliseconds(100), 5, 0.5f);
    
    // Only set lambda, norm_dist_var should remain unchanged
    bool set_success = manager->setTraderParameters("rlt_1", {{"lambda", 9.0}});
    ASSERT_TRUE(set_success);

    auto updated_params = manager->getTraderParameters("rlt_1");
    ASSERT_TRUE(updated_params.has_value());
    EXPECT_EQ(updated_params->at("lambda"), 9.0);
    EXPECT_EQ(updated_params->at("norm_dist_var"), 0.5); // Should be original value
}

TEST_F(TraderManagerTest, SetNonExistentTraderParams) {
    bool set_success = manager->setTraderParameters("non_existent", {{"lambda", 1.0}});
    ASSERT_FALSE(set_success);
}
