import sys
from pathlib import Path
import pytest

# --- Add this block to fix the import path ---
# Get the absolute path of the current test file
current_file = Path(__file__).resolve()
# Go up two levels to get the project root directory (tests/python_tests/ -> tests/ -> root)
project_root = current_file.parents[2]
# Add the project root to the beginning of the Python path
sys.path.insert(0, str(project_root))
# --- End of fix ---

# Now your regular imports will work
from portfolio import Portfolio, Position
import trading_core

# --- Pytest Fixture ---
# This fixture creates a clean portfolio instance for each test function that requests it.
@pytest.fixture
def fresh_portfolio():
    """Returns a new Portfolio instance with 100k cash for trader 1."""
    return Portfolio(cash="100000.00", trader_id=1)

# --- Helper Function ---
def create_mock_trade_event(
    symbol: str,
    price: int,
    quantity: int,
    aggressor_id: int,
    aggressor_side: trading_core.Side,
    resting_id: int = 2 # Default opponent trader ID
) -> trading_core.TradeExecutedEvent:
    """Helper to create a mock TradeExecutedEvent with common fields."""
    event = trading_core.TradeExecutedEvent()
    event.symbol = symbol
    event.price = price
    event.quantity = quantity
    event.aggressing_trader_id = aggressor_id
    event.aggressing_side = aggressor_side
    event.resting_trader_id = resting_id
    return event

# --- Core Functionality Tests ---

def test_portfolio_initialization(fresh_portfolio):
    """Tests that a portfolio is created with the correct starting state."""
    assert fresh_portfolio.cash == 100000.0
    assert fresh_portfolio.value == 100000.0
    assert not fresh_portfolio.holdings # Holdings should be empty

def test_on_fill_buy_to_open_new_position(fresh_portfolio):
    """Tests a BUY fill correctly opens a new position and updates state."""
    # ARRANGE: Create a buy event for 10 shares of AAPL at $150
    buy_event = create_mock_trade_event("AAPL", 15000, 10, 1, trading_core.Side.BUY)

    # ACT: Process the fill
    fresh_portfolio.on_fill(buy_event)

    # ASSERT: Check the new state
    assert fresh_portfolio.cash == 100000.0 - (150 * 10)
    assert "AAPL" in fresh_portfolio.holdings
    
    aapl_pos = fresh_portfolio.holdings["AAPL"]
    assert aapl_pos.quantity == 10
    assert aapl_pos.cost_basis == 150.0

def test_on_fill_buy_to_average_down(fresh_portfolio):
    """Tests that buying more of a stock at a lower price correctly updates the cost basis."""
    # ARRANGE: First, buy 10 shares at $200
    first_buy = create_mock_trade_event("MSFT", 20000, 10, 1, trading_core.Side.BUY)
    fresh_portfolio.on_fill(first_buy)
    
    # ARRANGE: Now, create an event to buy 10 more shares at a cheaper price of $100
    second_buy = create_mock_trade_event("MSFT", 10000, 10, 1, trading_core.Side.BUY)

    # ACT: Process the second fill
    fresh_portfolio.on_fill(second_buy)

    # ASSERT: Check the final state
    # Total cost = (10 * 200) + (10 * 100) = 2000 + 1000 = 3000
    # Total quantity = 10 + 10 = 20
    # Expected cost basis = 3000 / 20 = 150.0
    msft_pos = fresh_portfolio.holdings["MSFT"]
    assert msft_pos.quantity == 20
    assert msft_pos.cost_basis == 150.0
    assert fresh_portfolio.cash == 100000.0 - 3000

def test_on_fill_sell_to_partially_close(fresh_portfolio):
    """Tests that selling a portion of a position updates quantity but not cost basis."""
    # ARRANGE: Buy 20 shares at $50
    buy_event = create_mock_trade_event("AMD", 5000, 20, 1, trading_core.Side.BUY)
    fresh_portfolio.on_fill(buy_event)
    
    # ARRANGE: Create an event to sell 5 of those shares at $60
    sell_event = create_mock_trade_event("AMD", 6000, 5, 1, trading_core.Side.SELL)

    # ACT: Process the sell fill
    fresh_portfolio.on_fill(sell_event)

    # ASSERT: Check the final state
    # Cash = 100k - (20*50) + (5*60) = 100k - 1000 + 300 = 99300
    assert fresh_portfolio.cash == 99300.0
    
    amd_pos = fresh_portfolio.holdings["AMD"]
    assert amd_pos.quantity == 15 # 20 - 5
    assert amd_pos.cost_basis == 50.0 # Cost basis should NOT change on a sell

def test_on_fill_sell_to_fully_close(fresh_portfolio):
    """Tests that selling all shares of a position removes it from holdings."""
    # ARRANGE: Buy 10 shares at $100
    buy_event = create_mock_trade_event("TSLA", 10000, 10, 1, trading_core.Side.BUY)
    fresh_portfolio.on_fill(buy_event)
    
    # ARRANGE: Sell all 10 shares
    sell_event = create_mock_trade_event("TSLA", 12000, 10, 1, trading_core.Side.SELL)

    # ACT: Process the closing sell
    fresh_portfolio.on_fill(sell_event)

    # ASSERT: The position should be gone
    assert "TSLA" not in fresh_portfolio.holdings
    assert fresh_portfolio.cash == 100000.0 - (10 * 100) + (10 * 120) # 100200

# --- Edge Case Tests ---

def test_on_fill_ignores_event_for_other_traders(fresh_portfolio):
    """Tests that the portfolio correctly ignores a fill event for another trader."""
    # ARRANGE: Create an event where our trader (1) is not involved
    fresh_portfolio.holdings.clear()
    other_trader_event = create_mock_trade_event("NVDA", 30000, 5, 99, trading_core.Side.BUY, resting_id=100)
    
    # ACT: Process the fill
    fresh_portfolio.on_fill(other_trader_event)

    # ASSERT: The portfolio's state should be completely unchanged
    assert fresh_portfolio.cash == 100000.0
    assert not fresh_portfolio.holdings

def test_on_fill_handles_resting_side(fresh_portfolio):
    """Tests that the portfolio correctly processes a fill when its order was the resting one."""
    # ARRANGE: Our trader (1) is the resting side of a SELL order
    resting_fill = create_mock_trade_event("GOOG", 100000, 5, 99, trading_core.Side.BUY, resting_id=1)
    
    # ACT: Process the fill
    fresh_portfolio.on_fill(resting_fill)
    
    # ASSERT: Our portfolio should have sold 5 shares of GOOG
    # This assumes we had a pre-existing position, which this test doesn't set up.
    # The key is to test that the logic correctly identifies the side.
    # A more complete test would pre-load the position.
    # For now, let's just check that cash increased as expected for a sell.
    assert fresh_portfolio.cash == 100000.0 + (1000 * 5)

# --- Stress Test ---

def test_rapid_buy_sell_sequence(fresh_portfolio):
    """Simulates a rapid sequence of trades to ensure state remains consistent."""
    # Buy 100 @ 10
    fresh_portfolio.on_fill(create_mock_trade_event("SPY", 1000, 100, 1, trading_core.Side.BUY))
    assert fresh_portfolio.holdings["SPY"].quantity == 100
    assert fresh_portfolio.cash == 99000.0

    # Sell 50 @ 12
    fresh_portfolio.on_fill(create_mock_trade_event("SPY", 1200, 50, 1, trading_core.Side.SELL))
    assert fresh_portfolio.holdings["SPY"].quantity == 50
    assert fresh_portfolio.holdings["SPY"].cost_basis == 10.0
    assert fresh_portfolio.cash == 99600.0

    # Buy 25 @ 8 (averaging down)
    fresh_portfolio.on_fill(create_mock_trade_event("SPY", 800, 25, 1, trading_core.Side.BUY))
    # New cost basis = ((50*10) + (25*8)) / 75 = (500 + 200) / 75 = 700 / 75 = 9.333...
    assert fresh_portfolio.holdings["SPY"].quantity == 75
    assert round(fresh_portfolio.holdings["SPY"].cost_basis, 4) == 9.3333
    assert fresh_portfolio.cash == 99400.0

    # Sell 75 @ 15 (close out)
    fresh_portfolio.on_fill(create_mock_trade_event("SPY", 1500, 75, 1, trading_core.Side.SELL))
    assert "SPY" not in fresh_portfolio.holdings
    # Final cash = 99400 + (75 * 15) = 99400 + 1125 = 100525.0
    assert fresh_portfolio.cash == 100525.0
