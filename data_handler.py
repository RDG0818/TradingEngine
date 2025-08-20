import pandas as pd
import yfinance as yf
import os

if not os.path.isfile("data.csv"):
    data = yf.download("AAPL", start="2020-01-01", end="2023-12-31")
    if data is not None: data.to_csv("data.csv")

class CSVDataHandler:
    def __init__(self, csv_path, symbol) -> None:
        self.symbol = symbol
        self._raw_data = pd.read_csv(
            csv_path, 
            header=0,
            index_col='Date',
            parse_dates=True
        )
        self._data_stream = self._raw_data.iterrows()
        print(f"Data handler initialized for {self.symbol} with {len(self._raw_data)} bars.")
    
    def stream_bars(self):
        for index, row in self._data_stream:
            bar = {
                "timestamp": index,
                "symbol": self.symbol,
                "open": row['Open'],
                "high": row['High'],
                "low": row['Low'],
                "close": row['Close'],
                "volume": row['Volume']
            } 
            yield bar

