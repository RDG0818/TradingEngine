import React, { useState, useEffect } from 'react';
import { LineChart, Line, ResponsiveContainer, YAxis } from 'recharts';
import { MarketData, ChartPoint } from '../types';
import { useSymbol } from '../context/SymbolContext';
import { ChevronDown } from 'lucide-react';

const MarketSnapshot: React.FC = () => {
  const { availableSymbols, currentSymbol, setCurrentSymbol } = useSymbol();
  const [data, setData] = useState<MarketData>({
    bestBid: 0, bestAsk: 0, lastPrice: 0, lastVolume: 0, timestamp: Date.now(),
  });
  const [allChartData, setAllChartData] = useState<Record<string, ChartPoint[]>>({});

  useEffect(() => {
    if (!currentSymbol) return;

    const fetchMarketData = async () => {
      try {
        const response = await fetch(`http://localhost:8000/market_snapshot?symbol=${currentSymbol}`);
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
        const fetchedData: MarketData = await response.json();
        setData(fetchedData);
        
        setAllChartData(prevData => {
            const currentSymbolData = prevData[currentSymbol] || [];
            const newPoint = { time: Date.now(), price: fetchedData.lastPrice };
            const updatedSymbolData = [...currentSymbolData, newPoint].slice(-60);
            return {
                ...prevData,
                [currentSymbol]: updatedSymbolData,
            };
        });

      } catch (error) { console.error("Error fetching market snapshot:", error); }
    };

    fetchMarketData();
    const interval = setInterval(fetchMarketData, 800);
    return () => clearInterval(interval);
  }, [currentSymbol]);

  const isIdle = data.bestBid === 0 && data.bestAsk === 0;
  const spread = isIdle ? '0.00' : (data.bestAsk - data.bestBid).toFixed(2);
  const midPrice = isIdle ? '0.00' : ((data.bestAsk + data.bestBid) / 2).toFixed(2);
  
  const currentChartData = allChartData[currentSymbol] || [];

  const Metric = ({ label, value, color, unit }: { label: string; value: string; color?: string; unit?: string }) => (
    <div className="text-center">
        <label className="text-[10px] text-neutral-500 font-medium uppercase tracking-wider block mb-1">{label}</label>
        <div className="flex items-baseline justify-center gap-1">
            <span className={`text-lg font-mono tracking-tight ${color || 'text-neutral-200'}`}>{value}</span>
            {unit && <span className="text-xs text-neutral-500 font-mono">{unit}</span>}
        </div>
    </div>
  );

  return (
    <div className="bg-neutral-950/70 border border-white/5 rounded-lg h-full flex flex-col">
       <div className="p-3 border-b border-white/5 flex justify-center items-center relative">
            <h2 className="text-sm font-semibold text-neutral-300">Market Snapshot</h2>
            <div className="absolute right-3 flex items-center gap-2">
                <span className="text-xs font-mono text-neutral-500">TICKER:</span>
                <div className="relative">
                    <select
                        value={currentSymbol}
                        onChange={(e) => setCurrentSymbol(e.target.value)}
                        className="appearance-none bg-transparent border-none text-sm font-mono text-blue-400 font-bold pr-5 focus:outline-none"
                        disabled={availableSymbols.length === 0}
                    >
                        {availableSymbols.length > 0 ? (
                          availableSymbols.map(s => <option key={s} value={s} className="bg-neutral-800 text-white">{s}</option>)
                        ) : (
                          <option className="bg-neutral-800 text-white">Loading...</option>
                        )}
                    </select>
                    <ChevronDown size={14} className="absolute right-0 top-1/2 -translate-y-1/2 text-neutral-600 pointer-events-none" />
                </div>
            </div>
       </div>

       <div className="p-5 flex-1 flex flex-col justify-between gap-6">
            <div className="h-24 w-full relative chart-grid-bg">
                <ResponsiveContainer width="100%" height="100%">
                    <LineChart data={isIdle ? [{time: Date.now(), price: 0}] : currentChartData}>
                        <defs>
                            <linearGradient id="priceGradient" x1="0" y1="0" x2="0" y2="1">
                                <stop offset="5%" stopColor="#10b981" stopOpacity={0.3}/>
                                <stop offset="95%" stopColor="#10b981" stopOpacity={0}/>
                            </linearGradient>
                        </defs>
                        <Line type="monotone" dataKey="price" stroke={isIdle ? "#4b5563" : "#10b981"} strokeWidth={1.5} dot={false} isAnimationActive={false} strokeDasharray={isIdle ? "3 3" : "0"} />
                        <YAxis domain={['auto', 'auto']} hide />
                    </LineChart>
                </ResponsiveContainer>
                {isIdle && currentChartData.length === 0 && (
                    <div className="absolute inset-0 flex items-center justify-center">
                        <span className="text-xs font-mono text-neutral-500">Waiting for data feed...</span>
                    </div>
                )}
            </div>

            <div className="grid grid-cols-3 md:grid-cols-6 gap-4">
                <Metric label="Best Bid" value={isIdle ? '$0.00' : `$${data.bestBid.toFixed(2)}`} color="text-emerald-500" />
                <Metric label="Best Ask" value={isIdle ? '$0.00' : `$${data.bestAsk.toFixed(2)}`} color="text-rose-500" />
                <Metric label="Mid Price" value={`$${midPrice}`} />
                <Metric label="Spread" value={spread} color="text-neutral-400" />
                <Metric label="Volume (1m)" value={isIdle ? '0' : data.lastVolume.toLocaleString()} />
                <Metric label="Last Trade" value={isIdle ? '0.00' : data.lastPrice.toFixed(2)} />
            </div>
       </div>
    </div>
  );
};

export default MarketSnapshot;