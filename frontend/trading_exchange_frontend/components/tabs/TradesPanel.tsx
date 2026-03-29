import React from 'react';
import { useApp } from '../../context/AppContext';

const TradesPanel: React.FC = () => {
  const { trades } = useApp();

  const fmtPrice = (p: number) =>
    p.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 });

  const fmtTime = (ts: number) => {
    const d = new Date(ts);
    return `${d.getHours().toString().padStart(2,'0')}:${d.getMinutes().toString().padStart(2,'0')}:${d.getSeconds().toString().padStart(2,'0')}`;
  };

  if (trades.length === 0) {
    return (
      <div className="h-full flex items-center justify-center text-neutral-600 text-xs">
        Waiting for trades…
      </div>
    );
  }

  return (
    <div className="h-full flex flex-col">
      {/* Column headers */}
      <div className="flex justify-between px-3 py-1 text-[9px] uppercase tracking-wider text-neutral-600 border-b border-[#1a1a1a]">
        <span>Price</span>
        <span>Qty</span>
        <span>Time</span>
      </div>

      {/* Trade list — newest at top, scrollable */}
      <div className="flex-1 overflow-y-auto no-scrollbar">
        {trades.map((trade, i) => {
          // Color by price direction vs previous trade
          const prev = trades[i + 1];
          const isUp = !prev || trade.price >= prev.price;
          return (
            <div
              key={`${trade.timestamp}-${i}`}
              className="flex justify-between items-center px-3 py-[2px] text-[11px] hover:bg-[#111]"
            >
              <span className={isUp ? 'text-green-400' : 'text-red-400'}>
                {fmtPrice(trade.price)}
              </span>
              <span className="text-neutral-400">{trade.qty}</span>
              <span className="text-neutral-600">{fmtTime(trade.timestamp)}</span>
            </div>
          );
        })}
      </div>
    </div>
  );
};

export default TradesPanel;
