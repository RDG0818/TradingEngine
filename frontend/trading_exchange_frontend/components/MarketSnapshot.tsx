import React, { useState, useEffect, useMemo } from 'react';
import { LineChart, Line, ResponsiveContainer, YAxis } from 'recharts';
import { MarketData, ChartPoint } from '../types';

const MarketSnapshot: React.FC = () => {
  const [data, setData] = useState<MarketData>({
    bestBid: 123.45,
    bestAsk: 123.47,
    lastPrice: 123.46,
    lastVolume: 1234,
    timestamp: Date.now(),
  });

  const [chartData, setChartData] = useState<ChartPoint[]>([]);

  // Simulate market data updates
  useEffect(() => {
    // Initialize some chart data
    const initialChartData = Array.from({ length: 60 }, (_, i) => ({
      time: i,
      price: 123.40 + Math.random() * 0.2
    }));
    setChartData(initialChartData);

    const interval = setInterval(() => {
      setData(prev => {
        const volatility = 0.05;
        const change = (Math.random() - 0.5) * volatility;
        const newPrice = Math.max(0, prev.lastPrice + change);
        const spread = 0.02 + Math.random() * 0.01;
        
        return {
          bestBid: Number((newPrice - (spread/2)).toFixed(2)),
          bestAsk: Number((newPrice + (spread/2)).toFixed(2)),
          lastPrice: Number(newPrice.toFixed(2)),
          lastVolume: Math.floor(Math.random() * 5000) + 500,
          timestamp: Date.now()
        };
      });

      setChartData(prev => {
        const newData = [...prev.slice(1), { time: Date.now(), price: data.lastPrice }];
        return newData;
      });
    }, 800);

    return () => clearInterval(interval);
  }, [data.lastPrice]);

  const spread = (data.bestAsk - data.bestBid).toFixed(2);
  const midPrice = ((data.bestAsk + data.bestBid) / 2).toFixed(2);
  const formatTime = (ts: number) => new Date(ts).toISOString().split('T')[1].split('.')[0];

  return (
    <div className="bg-neutral-900 border border-neutral-800 rounded-lg p-1 h-full flex flex-col">
       <div className="p-4 border-b border-neutral-800 flex justify-between items-center">
            <h2 className="text-sm font-semibold text-neutral-300">Market Snapshot</h2>
            <div className="flex gap-2">
                <span className="text-xs font-mono text-neutral-500">TICKER:</span>
                <span className="text-xs font-mono text-blue-400 font-bold">ETH-USD-PERP</span>
            </div>
       </div>

       <div className="p-5 flex-1 flex flex-col gap-6">
            {/* Sparkline Area */}
            <div className="h-24 w-full relative">
                <ResponsiveContainer width="100%" height="100%">
                    <LineChart data={chartData}>
                        <Line 
                            type="monotone" 
                            dataKey="price" 
                            stroke="#10b981" 
                            strokeWidth={1.5} 
                            dot={false} 
                            isAnimationActive={false} 
                        />
                        <YAxis domain={['auto', 'auto']} hide />
                    </LineChart>
                </ResponsiveContainer>
                <div className="absolute top-0 right-0 bg-neutral-900/80 px-2 py-0.5 rounded text-[10px] text-neutral-500 border border-neutral-800">
                    1M INTERVAL
                </div>
            </div>

            {/* Data Grid */}
            <div className="grid grid-cols-2 gap-x-8 gap-y-6">
                <div>
                    <label className="text-xs text-neutral-500 font-medium uppercase tracking-wider block mb-1">Best Bid</label>
                    <span className="text-xl font-mono text-emerald-500 tracking-tight">{data.bestBid.toFixed(2)}</span>
                </div>
                <div>
                    <label className="text-xs text-neutral-500 font-medium uppercase tracking-wider block mb-1">Best Ask</label>
                    <span className="text-xl font-mono text-rose-500 tracking-tight">{data.bestAsk.toFixed(2)}</span>
                </div>
                <div>
                    <label className="text-xs text-neutral-500 font-medium uppercase tracking-wider block mb-1">Mid Price</label>
                    <span className="text-lg font-mono text-neutral-200">{midPrice}</span>
                </div>
                <div>
                    <label className="text-xs text-neutral-500 font-medium uppercase tracking-wider block mb-1">Spread</label>
                    <span className="text-lg font-mono text-neutral-400">{spread}</span>
                </div>
            </div>

            <div className="h-px bg-neutral-800 w-full my-1"></div>

            <div className="grid grid-cols-2 gap-4">
                 <div>
                    <label className="text-[10px] text-neutral-600 font-medium uppercase tracking-wider block mb-1">Volume (1m)</label>
                    <span className="text-sm font-mono text-neutral-300">{data.lastVolume.toLocaleString()}</span>
                </div>
                <div>
                    <label className="text-[10px] text-neutral-600 font-medium uppercase tracking-wider block mb-1">Last Trade</label>
                    <div className="flex flex-col">
                        <span className="text-sm font-mono text-neutral-200">{data.lastPrice.toFixed(2)}</span>
                        <span className="text-[10px] font-mono text-neutral-500">{formatTime(data.timestamp)}</span>
                    </div>
                </div>
            </div>
       </div>
    </div>
  );
};

export default MarketSnapshot;