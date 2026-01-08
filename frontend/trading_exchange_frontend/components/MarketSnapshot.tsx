import React, { useState, useEffect, useMemo } from 'react';
import { LineChart, Line, ResponsiveContainer, YAxis } from 'recharts';
import { MarketData, ChartPoint } from '../types';

const MarketSnapshot: React.FC = () => {
  const [data, setData] = useState<MarketData>({
    bestBid: 0,
    bestAsk: 0,
    lastPrice: 0,
    lastVolume: 0,
    timestamp: Date.now(),
  });

  const [chartData, setChartData] = useState<ChartPoint[]>([]);

  // Fetch market data from backend
  useEffect(() => {
    const fetchMarketData = async () => {
      try {
        const response = await fetch('http://localhost:8000/market_snapshot'); // Your FastAPI backend address
        if (!response.ok) {
          throw new Error(`HTTP error! status: ${response.status}`);
        }
        const fetchedData: MarketData = await response.json();
        setData(fetchedData);
        
        // Update chart data using the fetched lastPrice
        setChartData(prev => {
          const newChartPoint = { time: Date.now(), price: fetchedData.lastPrice };
          // Keep a fixed number of points for the chart, e.g., 60
          const updatedChartData = [...prev, newChartPoint].slice(-60); 
          return updatedChartData;
        });

      } catch (error) {
        console.error("Error fetching market snapshot:", error);
        // Optionally, reset data or show an error state on the UI
      }
    };

    // Fetch immediately on mount, and then every 800ms
    fetchMarketData();
    const interval = setInterval(fetchMarketData, 800);

    return () => clearInterval(interval); // Cleanup on component unmount
  }, []); // Empty dependency array to run once on mount for initial setup

  const isIdle = data.bestBid === 0 && data.bestAsk === 0;
  const spread = isIdle ? '0.00' : (data.bestAsk - data.bestBid).toFixed(2);
  const midPrice = isIdle ? '0.00' : ((data.bestAsk + data.bestBid) / 2).toFixed(2);
  const formatTime = (ts: number) => new Date(ts).toISOString().split('T')[1].split('.')[0];

  return (
    <div className="bg-neutral-50 dark:bg-neutral-900 border border-neutral-200 dark:border-neutral-700 rounded-lg p-1 h-full flex flex-col">
       <div className="p-4 border-b border-neutral-200 dark:border-neutral-700 flex justify-between items-center">
            <h2 className="text-sm font-semibold text-neutral-700 dark:text-neutral-300">Market Snapshot</h2>
            <div className="flex gap-2">
                <span className="text-xs font-mono text-neutral-500">TICKER:</span>
                <span className="text-xs font-mono text-blue-500 dark:text-blue-400 font-bold">ETH-USD-PERP</span>
            </div>
       </div>

       <div className="p-5 flex-1 flex flex-col gap-6">
            {/* Sparkline Area */}
            <div className="h-24 w-full relative chart-grid-bg">
                <ResponsiveContainer width="100%" height="100%">
                    <LineChart data={isIdle ? [{time: Date.now(), price: 0}] : chartData}>
                        <Line 
                            type="monotone" 
                            dataKey="price" 
                            stroke={isIdle ? "#9ca3af" : "#10b981"}
                            strokeWidth={1.5} 
                            dot={false} 
                            isAnimationActive={false}
                            strokeDasharray={isIdle ? "3 3" : "0"}
                        />
                        <YAxis domain={['auto', 'auto']} hide />
                    </LineChart>
                </ResponsiveContainer>
                {isIdle && (
                    <div className="absolute inset-0 flex items-center justify-center">
                        <span className="text-xs font-mono text-neutral-500">Waiting for data feed...</span>
                    </div>
                )}
                <div className="absolute top-0 right-0 bg-white/80 dark:bg-neutral-950/80 px-2 py-0.5 rounded text-[10px] text-neutral-500 border border-neutral-200 dark:border-neutral-700">
                    1M INTERVAL
                </div>
            </div>

            {/* Data Grid */}
            <div className="grid grid-cols-2 gap-x-8 gap-y-6">
                <div>
                    <label className="text-xs text-neutral-500 font-medium uppercase tracking-wider block mb-1">Best Bid</label>
                    <span className="text-xl font-mono text-emerald-600 dark:text-emerald-500 tracking-tight">{isIdle ? '0.00' : data.bestBid.toFixed(2)}</span>
                </div>
                <div>
                    <label className="text-xs text-neutral-500 font-medium uppercase tracking-wider block mb-1">Best Ask</label>
                    <span className="text-xl font-mono text-rose-600 dark:text-rose-500 tracking-tight">{isIdle ? '0.00' : data.bestAsk.toFixed(2)}</span>
                </div>
                <div>
                    <label className="text-xs text-neutral-500 font-medium uppercase tracking-wider block mb-1">Mid Price</label>
                    <span className="text-lg font-mono text-neutral-800 dark:text-neutral-200">{midPrice}</span>
                </div>
                <div>
                    <label className="text-xs text-neutral-500 font-medium uppercase tracking-wider block mb-1">Spread</label>
                    <span className="text-lg font-mono text-neutral-600 dark:text-neutral-400">{spread}</span>
                </div>
            </div>

            <div className="h-px bg-neutral-200 dark:bg-neutral-700 w-full my-1"></div>

            <div className="grid grid-cols-2 gap-4">
                 <div>
                    <label className="text-[10px] text-neutral-500 dark:text-neutral-600 font-medium uppercase tracking-wider block mb-1">Volume (1m)</label>
                    <span className="text-sm font-mono text-neutral-700 dark:text-neutral-300">{isIdle ? '0' : data.lastVolume.toLocaleString()}</span>
                </div>
                <div>
                    <label className="text-[10px] text-neutral-500 dark:text-neutral-600 font-medium uppercase tracking-wider block mb-1">Last Trade</label>
                    <div className="flex flex-col">
                        <span className="text-sm font-mono text-neutral-800 dark:text-neutral-200">{isIdle ? '0.00' : data.lastPrice.toFixed(2)}</span>
                        <span className="text-[10px] font-mono text-neutral-500">{isIdle ? '...' : formatTime(data.timestamp)}</span>
                    </div>
                </div>
            </div>
       </div>
    </div>
  );
};

export default MarketSnapshot;