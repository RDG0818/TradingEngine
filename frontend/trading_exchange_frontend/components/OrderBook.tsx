import React, { useMemo } from 'react';
import { useApp } from '../context/AppContext';

const LEVELS = 15;

function computeDepthBars(levels: [number, number][]): { price: number; qty: number; cum: number; barPct: number }[] {
  let cum = 0;
  const rows = levels.slice(0, LEVELS).map(([price, qty]) => {
    cum += qty;
    return { price, qty, cum, barPct: 0 };
  });
  const maxCum = rows[rows.length - 1]?.cum || 1;
  return rows.map(r => ({ ...r, barPct: (r.cum / maxCum) * 100 }));
}

const OrderBook: React.FC = () => {
  const { book, lastTradePrice } = useApp();

  // Asks: ascending price, display top-to-bottom reversed (closest ask at bottom)
  const asks = useMemo(() => {
    const sorted = [...book.asks].sort((a, b) => a[0] - b[0]);
    return computeDepthBars(sorted).reverse();
  }, [book.asks]);

  // Bids: descending price (best bid first)
  const bids = useMemo(() => {
    const sorted = [...book.bids].sort((a, b) => b[0] - a[0]);
    return computeDepthBars(sorted);
  }, [book.bids]);

  const spread = useMemo(() => {
    const bestAsk = book.asks.reduce((min, [p]) => p < min ? p : min, Infinity);
    const bestBid = book.bids.reduce((max, [p]) => p > max ? p : max, 0);
    if (bestAsk === Infinity || bestBid === 0) return null;
    return (bestAsk - bestBid).toFixed(2);
  }, [book]);

  const fmtPrice = (p: number) =>
    p.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 });

  return (
    <div className="flex flex-col h-full text-[11px] select-none">

      {/* Column headers */}
      <div className="flex justify-between text-neutral-600 px-2 py-1 text-[9px] uppercase tracking-wider border-b border-[#1a1a1a]">
        <span>Price</span>
        <span>Qty</span>
        <span>Total</span>
      </div>

      {/* Asks (sells) — top, red */}
      <div className="flex-1 flex flex-col justify-end overflow-hidden px-2 py-1 gap-[1px]">
        {asks.map(row => (
          <div key={row.price} className="relative flex justify-between items-center py-[1px] px-[2px]">
            {/* Depth bar — grows from right */}
            <div
              className="absolute right-0 top-0 bottom-0 bg-red-500/10 rounded-sm"
              style={{ width: `${row.barPct}%` }}
            />
            <span className="relative text-red-400">{fmtPrice(row.price)}</span>
            <span className="relative text-red-400/70">{row.qty}</span>
            <span className="relative text-red-400/50">{row.cum}</span>
          </div>
        ))}
      </div>

      {/* Spread row */}
      <div className="flex items-center justify-between px-3 py-[6px] border-y border-[#1a1a1a] bg-[#0f0f0f]">
        <span className="text-green-400 text-[13px] font-medium">
          {lastTradePrice ? `$${fmtPrice(lastTradePrice)}` : '—'}
        </span>
        {spread && (
          <span className="text-neutral-600 text-[9px]">spread ${spread}</span>
        )}
      </div>

      {/* Bids (buys) — bottom, green */}
      <div className="flex-1 overflow-hidden px-2 py-1 flex flex-col gap-[1px]">
        {bids.map(row => (
          <div key={row.price} className="relative flex justify-between items-center py-[1px] px-[2px]">
            {/* Depth bar — grows from left */}
            <div
              className="absolute left-0 top-0 bottom-0 bg-green-500/10 rounded-sm"
              style={{ width: `${row.barPct}%` }}
            />
            <span className="relative text-green-400">{fmtPrice(row.price)}</span>
            <span className="relative text-green-400/70">{row.qty}</span>
            <span className="relative text-green-400/50">{row.cum}</span>
          </div>
        ))}
      </div>

    </div>
  );
};

export default OrderBook;
