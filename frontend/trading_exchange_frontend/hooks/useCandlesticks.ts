import { useState, useCallback } from 'react';
import { Candle, TradeWithTimestamp } from '../types';

const MAX_CANDLES = 60;

function bucketKey(timestamp: number, intervalMs: number): number {
  return Math.floor(timestamp / intervalMs) * intervalMs;
}

export function useCandlesticks(intervalMs = 10_000): {
  candles: Candle[];
  addTrade: (trade: TradeWithTimestamp) => void;
  seedTrades: (trades: TradeWithTimestamp[]) => void;
} {
  const [candles, setCandles] = useState<Candle[]>([]);

  const addTrade = useCallback((trade: TradeWithTimestamp) => {
    const bucket = bucketKey(trade.timestamp, intervalMs);

    setCandles(prev => {
      const last = prev[prev.length - 1];

      if (last && last.time === bucket) {
        // Extend current candle
        const updated: Candle = {
          ...last,
          high: Math.max(last.high, trade.price),
          low: Math.min(last.low, trade.price),
          close: trade.price,
          volume: last.volume + trade.qty,
        };
        return [...prev.slice(0, -1), updated];
      }

      // New candle
      const newCandle: Candle = {
        time: bucket,
        open: trade.price,
        high: trade.price,
        low: trade.price,
        close: trade.price,
        volume: trade.qty,
      };

      const updated = [...prev, newCandle];
      return updated.length > MAX_CANDLES ? updated.slice(-MAX_CANDLES) : updated;
    });
  }, [intervalMs]);

  const seedTrades = useCallback((trades: TradeWithTimestamp[]) => {
    if (trades.length === 0) return;

    // Build candle map from seed trades
    const map = new Map<number, Candle>();
    for (const trade of trades) {
      const bucket = bucketKey(trade.timestamp, intervalMs);
      const existing = map.get(bucket);
      if (!existing) {
        map.set(bucket, {
          time: bucket,
          open: trade.price,
          high: trade.price,
          low: trade.price,
          close: trade.price,
          volume: trade.qty,
        });
      } else {
        map.set(bucket, {
          ...existing,
          high: Math.max(existing.high, trade.price),
          low: Math.min(existing.low, trade.price),
          close: trade.price,
          volume: existing.volume + trade.qty,
        });
      }
    }

    const sorted = Array.from(map.values())
      .sort((a, b) => a.time - b.time)
      .slice(-MAX_CANDLES);

    setCandles(sorted);
  }, [intervalMs]);

  return { candles, addTrade, seedTrades };
}
