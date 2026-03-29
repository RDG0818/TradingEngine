import React, {
  createContext,
  useContext,
  useEffect,
  useRef,
  useState,
  useCallback,
} from 'react';
import { BookSnapshot, TradeWithTimestamp } from '../types';
import { useWebSocket, WsStatus } from '../hooks/useWebSocket';
import { useCandlesticks } from '../hooks/useCandlesticks';
import type { Candle } from '../types';

const API = 'http://localhost:8000';
const WS_URL = 'ws://localhost:8000/ws';

interface AppContextValue {
  // Real-time data
  book: BookSnapshot;
  trades: TradeWithTimestamp[];       // last 200, newest first
  candles: Candle[];
  wsStatus: WsStatus;
  lastTradePrice: number | null;

  // User portfolio
  userTraderId: number | null;

  // Helpers
  apiBase: string;
}

const defaultBook: BookSnapshot = { bids: [], asks: [] };

const AppContext = createContext<AppContextValue>({
  book: defaultBook,
  trades: [],
  candles: [],
  wsStatus: 'connecting',
  lastTradePrice: null,
  userTraderId: null,
  apiBase: API,
});

export function AppProvider({ children }: { children: React.ReactNode }) {
  const [book, setBook] = useState<BookSnapshot>(defaultBook);
  const [trades, setTrades] = useState<TradeWithTimestamp[]>([]);
  const [wsStatus, setWsStatus] = useState<WsStatus>('connecting');
  const [userTraderId, setUserTraderId] = useState<number | null>(null);
  const lastTradePriceRef = useRef<number | null>(null);
  const [lastTradePrice, setLastTradePrice] = useState<number | null>(null);

  const { candles, addTrade, seedTrades } = useCandlesticks(10_000);

  // Fetch initial data on mount
  useEffect(() => {
    // Seed order book
    fetch(`${API}/book_snapshot`)
      .then(r => r.json())
      .then((data: BookSnapshot) => setBook(data))
      .catch(() => {/* backend may not be up yet */});

    // Seed trades + candles
    fetch(`${API}/recent_trades?limit=100`)
      .then(r => r.json())
      .then((data: any[]) => {
        const now = Date.now();
        // Space seed trades 300ms apart going backwards
        const stamped: TradeWithTimestamp[] = data.map((t, i) => ({
          price: t.price,
          qty: t.qty,
          maker_order_id: t.maker_order_id,
          taker_order_id: t.taker_order_id,
          maker_trader_id: t.maker_trader_id,
          taker_trader_id: t.taker_trader_id,
          timestamp: now - (data.length - i) * 300,
        }));
        setTrades(stamped.slice().reverse()); // newest first
        seedTrades(stamped);

        if (stamped.length > 0) {
          const last = stamped[stamped.length - 1].price;
          lastTradePriceRef.current = last;
          setLastTradePrice(last);
        }
      })
      .catch(() => {});

    // Get user portfolio ID
    fetch(`${API}/portfolio/default_id`)
      .then(r => r.json())
      .then((data: { trader_id: number }) => setUserTraderId(data.trader_id))
      .catch(() => {});
  }, [seedTrades]);

  // Handle incoming WS messages
  const handleMessage = useCallback((event: MessageEvent) => {
    try {
      const msg = JSON.parse(event.data as string);

      if (msg.type === 'book_update') {
        setBook(msg.data as BookSnapshot);
      } else if (msg.type === 'trade') {
        const trade: TradeWithTimestamp = {
          ...msg.data,
          timestamp: Date.now(),
        };
        lastTradePriceRef.current = trade.price;
        setLastTradePrice(trade.price);
        setTrades(prev => {
          const next = [trade, ...prev];
          return next.length > 200 ? next.slice(0, 200) : next;
        });
        addTrade(trade);
      }
    } catch {
      // malformed message — ignore
    }
  }, [addTrade]);

  useWebSocket(WS_URL, handleMessage, setWsStatus);

  return (
    <AppContext.Provider value={{
      book,
      trades,
      candles,
      wsStatus,
      lastTradePrice,
      userTraderId,
      apiBase: API,
    }}>
      {children}
    </AppContext.Provider>
  );
}

export function useApp(): AppContextValue {
  return useContext(AppContext);
}
