export interface BookSnapshot {
  bids: [number, number][];  // [price_usd, qty]
  asks: [number, number][];
}

export interface Trade {
  price: number;
  qty: number;
  maker_order_id: number;
  taker_order_id: number;
  maker_trader_id: number;
  taker_trader_id: number;
}

export interface TradeWithTimestamp extends Trade {
  timestamp: number;  // Date.now() assigned client-side
}

export interface Candle {
  time: number;   // bucket start (ms), used as XAxis key
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

export interface TraderMetrics {
  orders_per_second: number;
  pnl: number;       // fixed-point: divide by 10000 for USD
  position: number;  // integer (# of BTC units)
}

export interface Trader {
  id: number;
  name: string;
  type: string;
  active: boolean;
  metrics: TraderMetrics;
}

export interface MarketEvent {
  id: string;
  name: string;
  description: string;
  default_duration_s: number;
}

export interface PortfolioSnapshot {
  balance: number;       // fixed-point: divide by 10000 for USD
  balance_usd: number;   // USD float
  position: number;      // integer BTC units
  unrealized_pnl: number; // fixed-point: divide by 10000 for USD
  avg_cost: number;      // USD float
}

export interface Metrics {
  orders_processed: number;
  avg_latency_us: number;
  throughput_per_s: number;
  last_trade_price: number;  // USD float
}

export type TabId = 'trades' | 'traders' | 'events' | 'portfolio';
