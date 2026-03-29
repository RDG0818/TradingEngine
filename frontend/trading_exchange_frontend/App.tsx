import React, { useEffect, useState } from 'react';
import { useApp } from './context/AppContext';
import { TabId } from './types';
import OrderBook from './components/OrderBook';
import CandlestickChart from './components/CandlestickChart';

const TABS: { id: TabId; label: string }[] = [
  { id: 'trades',    label: 'Trades'    },
  { id: 'traders',   label: 'Traders'   },
  { id: 'events',    label: 'Events'    },
  { id: 'portfolio', label: 'Portfolio' },
];

const App: React.FC = () => {
  const { wsStatus, lastTradePrice } = useApp();
  const [activeTab, setActiveTab] = useState<TabId>('trades');

  useEffect(() => {
    document.documentElement.classList.add('dark');
  }, []);

  const statusColor =
    wsStatus === 'connected'    ? 'text-green-400' :
    wsStatus === 'connecting'   ? 'text-yellow-400' :
                                  'text-red-400';

  return (
    <div className="flex h-screen w-full bg-[#0a0a0a] text-neutral-200 font-mono overflow-hidden">

      {/* ── Left panel: Order Book ── */}
      <div className="w-[32%] flex-shrink-0 h-full border-r border-[#1a1a1a] flex flex-col">
        {/* Header */}
        <div className="px-3 py-2 border-b border-[#1a1a1a] flex items-center justify-between">
          <span className="text-[10px] text-neutral-500 uppercase tracking-wider">Order Book</span>
          <span className={`text-[9px] ${statusColor}`}>
            {wsStatus === 'connected' ? '● live' : wsStatus === 'connecting' ? '○ connecting' : '○ disconnected'}
          </span>
        </div>
        <div className="flex-1 overflow-hidden">
          <OrderBook />
        </div>
      </div>

      {/* ── Right panel ── */}
      <div className="flex-1 flex flex-col h-full overflow-hidden">

        {/* Price header */}
        <div className="px-3 py-2 border-b border-[#1a1a1a] flex items-center gap-3">
          <span className="text-[11px] text-neutral-400">BTC/USD</span>
          <span className="text-sm font-medium text-green-400">
            {lastTradePrice ? `$${lastTradePrice.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}` : '—'}
          </span>
        </div>

        {/* Candlestick chart */}
        <div className="h-[35%] flex-shrink-0 border-b border-[#1a1a1a]">
          <CandlestickChart />
        </div>

        {/* Tab bar */}
        <div className="flex border-b border-[#1a1a1a] flex-shrink-0">
          {TABS.map(tab => (
            <button
              key={tab.id}
              onClick={() => setActiveTab(tab.id)}
              className={`flex-1 py-2 text-[10px] uppercase tracking-wider transition-colors
                ${activeTab === tab.id
                  ? 'text-green-400 border-b border-green-400'
                  : 'text-neutral-500 hover:text-neutral-300'}`}
            >
              {tab.label}
            </button>
          ))}
        </div>

        {/* Tab content */}
        <div className="flex-1 overflow-hidden">
          <div className="text-neutral-600 text-xs p-4">{activeTab}</div>
        </div>

      </div>
    </div>
  );
};

export default App;