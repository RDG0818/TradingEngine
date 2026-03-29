import React, { useEffect, useState, useCallback } from 'react';
import { Trader } from '../../types';
import { useApp } from '../../context/AppContext';

const TYPE_ABBREV: Record<string, string> = {
  market_maker:    'MM',
  momentum:        'MOM',
  mean_reversion:  'MR',
  twap:            'TWAP',
  trend_follower:  'TF',
  random_limit:    'RL',
  random_market:   'RM',
  panic:           'PAN',
};

const TradersPanel: React.FC = () => {
  const { apiBase } = useApp();
  const [traders, setTraders] = useState<Trader[]>([]);
  const [toggling, setToggling] = useState<Set<number>>(new Set());

  const fetchTraders = useCallback(() => {
    fetch(`${apiBase}/traders`)
      .then(r => r.json())
      .then(data => setTraders(data.traders ?? []))
      .catch(() => {});
  }, [apiBase]);

  // Initial fetch + polling every 2s
  useEffect(() => {
    fetchTraders();
    const id = setInterval(fetchTraders, 2000);
    return () => clearInterval(id);
  }, [fetchTraders]);

  const handleToggle = async (traderId: number) => {
    setToggling(prev => new Set(prev).add(traderId));
    try {
      await fetch(`${apiBase}/traders/${traderId}/toggle`, { method: 'POST' });
      await fetchTraders();
    } finally {
      setToggling(prev => {
        const next = new Set(prev);
        next.delete(traderId);
        return next;
      });
    }
  };

  const fmtPnl = (raw: number) => {
    const usd = raw / 10000;
    const sign = usd >= 0 ? '+' : '';
    return `${sign}$${Math.abs(usd).toFixed(0)}`;
  };

  if (traders.length === 0) {
    return (
      <div className="h-full flex items-center justify-center text-neutral-600 text-xs">
        Loading traders…
      </div>
    );
  }

  return (
    <div className="h-full flex flex-col">
      {/* Column headers */}
      <div className="flex items-center px-3 py-1 text-[9px] uppercase tracking-wider text-neutral-600 border-b border-[#1a1a1a]">
        <span className="w-[110px]">Name</span>
        <span className="w-[40px]">Type</span>
        <span className="w-[64px] text-right">PnL</span>
        <span className="w-[48px] text-right">Fills</span>
        <span className="ml-auto">On</span>
      </div>

      {/* Trader rows */}
      <div className="flex-1 overflow-y-auto no-scrollbar">
        {traders.map(trader => {
          const isToggling = toggling.has(trader.id);
          const pnlUsd = trader.metrics.pnl / 10000;
          return (
            <div
              key={trader.id}
              className={`flex items-center px-3 py-[5px] text-[11px] border-b border-[#111] hover:bg-[#111] transition-opacity
                ${trader.active ? 'opacity-100' : 'opacity-40'}`}
            >
              <span className="w-[110px] text-neutral-200 truncate">{trader.name}</span>
              <span className="w-[40px] text-neutral-500">{TYPE_ABBREV[trader.type] ?? trader.type.slice(0,4).toUpperCase()}</span>
              <span className={`w-[64px] text-right ${pnlUsd >= 0 ? 'text-green-400' : 'text-red-400'}`}>
                {fmtPnl(trader.metrics.pnl)}
              </span>
              <span className="w-[48px] text-right text-neutral-500">
                {trader.metrics.orders_per_second.toFixed(0)}
              </span>
              {/* Toggle switch */}
              <button
                className="ml-auto"
                onClick={() => handleToggle(trader.id)}
                disabled={isToggling}
                aria-label={trader.active ? 'Stop trader' : 'Start trader'}
              >
                <div className={`w-8 h-4 rounded-full relative transition-colors
                  ${trader.active ? 'bg-green-500/70' : 'bg-neutral-700'}
                  ${isToggling ? 'opacity-50 cursor-not-allowed' : 'cursor-pointer'}`}
                >
                  <div className={`absolute top-[2px] w-3 h-3 rounded-full bg-white transition-transform
                    ${trader.active ? 'translate-x-4' : 'translate-x-[2px]'}`}
                  />
                </div>
              </button>
            </div>
          );
        })}
      </div>
    </div>
  );
};

export default TradersPanel;
