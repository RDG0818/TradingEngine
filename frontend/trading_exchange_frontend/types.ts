export interface MarketData {
    bestBid: number;
    bestAsk: number;
    lastPrice: number;
    lastVolume: number;
    timestamp: number;
  }
  
  export interface SystemMetrics {
    ordersProcessed: number;
    avgLatency: number;
    activeOrders: number;
    eventQueueDepth: number;
    throughput: number;
  }
  
  export type ExchangeMode = 'Manual Trading' | 'Simulation';
  
  export interface ChartPoint {
    time: number;
    price: number;
  }
  
export interface PortfolioData {
  balance: number;
  positions: { [symbol: string]: number };
  unrealizedPnl: number;
}