import pandas as pd
import quantstats as qs
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from data_handler import data_file_path # Path to your price data

def generate_quantstats_report(history_csv="csv/backtest_history.csv", report_filename="strategy_report.html"):
    """
    Generates a comprehensive quantitative analysis report using quantstats.
    """
    print("Generating QuantStats report...")
    
    # --- 1. Load and Prepare the History Data ---
    history = pd.read_csv(history_csv, parse_dates=['timestamp'])
    history.set_index('timestamp', inplace=True)
    
    # --- NEW: Resample to Daily Frequency ---
    # 'D' stands for daily. .last() gets the last value of the day.
    # .ffill() fills weekends/holidays with the last known value.
    daily_history = history['value'].resample('D').last().ffill()
    
    # Convert the clean, daily portfolio value to a returns series.
    returns = daily_history.pct_change().fillna(0)
    
    # --- Safeguard against runs with no trades/returns ---
    if returns.abs().sum() == 0:
        print("\nWARNING: No trades were made or no profit/loss was realized.")
        print("Skipping QuantStats report generation to avoid errors.\n")
        return

    # --- 2. Generate the Report ---
    # We'll benchmark against SPY (S&P 500 ETF)
    qs.reports.html(returns, benchmark='SPY', output=report_filename, title='MA Crossover Strategy on AAPL')
    
    print(f"QuantStats report saved to {report_filename}")

def generate_interactive_plot(trades_csv="csv/backtest_trades.csv", plot_filename="interactive_trades.html"):
    """
    Generates an interactive plot with price data and trade markers using Plotly.
    """
    print("Generating interactive Plotly chart...")

    # --- Load Data ---
    trades = pd.read_csv(trades_csv, parse_dates=['timestamp'])
    price_data = pd.read_csv(data_file_path, parse_dates=True, index_col=0)
    price_data.rename(columns=str.lower, inplace=True)

    # --- Create Candlestick Chart ---
    fig = make_subplots(rows=1, cols=1)
    fig.add_trace(go.Candlestick(x=price_data.index,
                                open=price_data['open'],
                                high=price_data['high'],
                                low=price_data['low'],
                                close=price_data['close'],
                                name='AAPL Price'))

    # --- Add Trade Markers ---
    buy_trades = trades[trades['side'] == 'BUY']
    sell_trades = trades[trades['side'] == 'SELL']

    fig.add_trace(go.Scatter(x=buy_trades['timestamp'], y=buy_trades['price'],
                             mode='markers',
                             marker=dict(color='green', size=10, symbol='triangle-up'),
                             name='Buy Signal'))
    
    fig.add_trace(go.Scatter(x=sell_trades['timestamp'], y=sell_trades['price'],
                             mode='markers',
                             marker=dict(color='red', size=10, symbol='triangle-down'),
                             name='Sell Signal'))

    # --- Customize Layout ---
    fig.update_layout(
        title='Interactive Trade Analysis for MA Crossover Strategy',
        yaxis_title='Price ($)',
        xaxis_title='Date',
        xaxis_rangeslider_visible=False, # Hide the range slider
        template='plotly_dark'
    )

    fig.write_html(plot_filename)
    print(f"Interactive plot saved to {plot_filename}")


if __name__ == "__main__":
    # This allows you to run the analysis independently after a backtest
    generate_quantstats_report()
    generate_interactive_plot()