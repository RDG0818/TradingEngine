import pandas as pd
import yfinance as yf
import os
from pathlib import Path

script_dir = Path(__file__).parent
data_file_path = script_dir / "data.csv"

if not data_file_path.is_file():
    print(f"Data file not found at {data_file_path}, downloading...")
    try:
        data = yf.download("AAPL", start="2020-01-01", end="2023-12-31")
        if data is not None and not data.empty:
            data.to_csv(data_file_path)
            print("Download complete.")
        else:
            print("Failed to download data.")
    except Exception as e:
        print(f"An error occurred during download: {e}")

class CSVDataHandler:
    def __init__(self, csv_path, symbol) -> None:
        self.symbol = symbol
        self._raw_data = pd.read_csv(
            csv_path, 
            header=0,
            skiprows=[1, 2],
            index_col=0,
            parse_dates=True
        )
        self._raw_data.rename(columns=str.lower, inplace=True)
        self._data_stream = self._raw_data.iterrows()
        print(f"Data handler initialized for {self.symbol} with {len(self._raw_data)} bars.")
    
    def stream_bars(self):
        """Generator function to yield each bar from the data."""
        return self._data_stream

