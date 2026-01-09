import React, { useState, useEffect } from 'react';
import { useSymbol } from '../../context/SymbolContext';
import { PortfolioData } from '../../types';

type OrderType = 'Market' | 'Limit' | 'Stop Market' | 'Stop Limit';

// Enhanced trading controls component
const TradingControls = () => {
    const [side, setSide] = useState<'buy' | 'sell'>('buy');
    const [orderType, setOrderType] = useState<OrderType>('Limit');

    const renderInputs = () => {
        switch (orderType) {
            case 'Market':
                return (
                    <div>
                        <label className="text-xs text-neutral-500 dark:text-neutral-400">Amount (BTC)</label>
                        <input type="number" placeholder="0.5" className="w-full mt-1 p-2 rounded-md bg-neutral-100 dark:bg-neutral-800 border border-neutral-200 dark:border-neutral-700 focus:ring-2 focus:ring-blue-500 outline-none"/>
                    </div>
                );
            case 'Limit':
                return (
                    <>
                        <div>
                            <label className="text-xs text-neutral-500 dark:text-neutral-400">Price (USD)</label>
                            <input type="number" placeholder="60,000.00" className="w-full mt-1 p-2 rounded-md bg-neutral-100 dark:bg-neutral-800 border border-neutral-200 dark:border-neutral-700 focus:ring-2 focus:ring-blue-500 outline-none"/>
                        </div>
                        <div>
                            <label className="text-xs text-neutral-500 dark:text-neutral-400">Amount (BTC)</label>
                            <input type="number" placeholder="0.5" className="w-full mt-1 p-2 rounded-md bg-neutral-100 dark:bg-neutral-800 border border-neutral-200 dark:border-neutral-700 focus:ring-2 focus:ring-blue-500 outline-none"/>
                        </div>
                    </>
                );
            case 'Stop Market':
                return (
                     <>
                        <div>
                            <label className="text-xs text-neutral-500 dark:text-neutral-400">Stop Price (USD)</label>
                            <input type="number" placeholder="59,500.00" className="w-full mt-1 p-2 rounded-md bg-neutral-100 dark:bg-neutral-800 border border-neutral-200 dark:border-neutral-700 focus:ring-2 focus:ring-blue-500 outline-none"/>
                        </div>
                        <div>
                            <label className="text-xs text-neutral-500 dark:text-neutral-400">Amount (BTC)</label>
                            <input type="number" placeholder="0.5" className="w-full mt-1 p-2 rounded-md bg-neutral-100 dark:bg-neutral-800 border border-neutral-200 dark:border-neutral-700 focus:ring-2 focus:ring-blue-500 outline-none"/>
                        </div>
                    </>
                );
            case 'Stop Limit':
                return (
                    <>
                        <div>
                            <label className="text-xs text-neutral-500 dark:text-neutral-400">Stop Price (USD)</label>
                            <input type="number" placeholder="59,500.00" className="w-full mt-1 p-2 rounded-md bg-neutral-100 dark:bg-neutral-800 border border-neutral-200 dark:border-neutral-700 focus:ring-2 focus:ring-blue-500 outline-none"/>
                        </div>
                        <div>
                            <label className="text-xs text-neutral-500 dark:text-neutral-400">Limit Price (USD)</label>
                            <input type="number" placeholder="59,450.00" className="w-full mt-1 p-2 rounded-md bg-neutral-100 dark:bg-neutral-800 border border-neutral-200 dark:border-neutral-700 focus:ring-2 focus:ring-blue-500 outline-none"/>
                        </div>
                        <div>
                            <label className="text-xs text-neutral-500 dark:text-neutral-400">Amount (BTC)</label>
                            <input type="number" placeholder="0.5" className="w-full mt-1 p-2 rounded-md bg-neutral-100 dark:bg-neutral-800 border border-neutral-200 dark:border-neutral-700 focus:ring-2 focus:ring-blue-500 outline-none"/>
                        </div>
                    </>
                );
        }
    }
  
    return (
      <div className="bg-neutral-50 dark:bg-neutral-900 border border-neutral-200 dark:border-neutral-700 rounded-lg flex flex-col">
        <div className="p-4 border-b border-neutral-200 dark:border-neutral-700">
          <h2 className="text-sm font-semibold text-neutral-700 dark:text-neutral-300 text-center">Trade</h2>
        </div>
        <div className="p-4 flex-1">
            <div className="grid grid-cols-2 gap-1 bg-neutral-200 dark:bg-neutral-800 rounded-md p-1 mb-4">
                <button 
                    onClick={() => setSide('buy')}
                    className={`py-2 text-sm font-semibold rounded-[5px] ${side === 'buy' ? 'bg-emerald-500 text-white' : 'text-neutral-600 dark:text-neutral-300 hover:bg-neutral-300/50 dark:hover:bg-neutral-700/50'}`}
                >
                    Buy
                </button>
                <button 
                    onClick={() => setSide('sell')}
                    className={`py-2 text-sm font-semibold rounded-[5px] ${side === 'sell' ? 'bg-rose-500 text-white' : 'text-neutral-600 dark:text-neutral-300 hover:bg-neutral-300/50 dark:hover:bg-neutral-700/50'}`}
                >
                    Sell
                </button>
            </div>

            {/* Order Type Selector */}
            <div className="grid grid-cols-4 gap-1 text-xs mb-4">
                {(['Limit', 'Market', 'Stop Market', 'Stop Limit'] as OrderType[]).map(type => (
                    <button key={type} onClick={() => setOrderType(type)} className={`py-1 rounded ${orderType === type ? 'text-blue-500 font-semibold' : 'text-neutral-500 hover:text-blue-500/80'}`}>
                        {type}
                    </button>
                ))}
            </div>

            <div className="space-y-4">
                {renderInputs()}
                <button className={`w-full py-3 rounded-md text-white font-semibold ${side === 'buy' ? 'bg-emerald-600 hover:bg-emerald-700' : 'bg-rose-600 hover:bg-rose-700'}`}>
                    {side === 'buy' ? 'Buy BTC' : 'Sell BTC'}
                </button>
            </div>
        </div>
      </div>
    );
};

// Copied from the main Portfolio widget
const PortfolioWidget: React.FC = () => {
    const [portfolio, setPortfolio] = useState<PortfolioData | null>(null);
    const [isLoading, setIsLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        const fetchPortfolio = async () => {
            try {
                const idResponse = await fetch('http://localhost:8000/trader/default_id');
                if (!idResponse.ok) throw new Error('Failed to fetch default trader ID');
                const { traderId } = await idResponse.json();
                if (traderId === null || traderId === undefined) return;
                const portfolioResponse = await fetch(`http://localhost:8000/portfolio/${traderId}`);
                if (!portfolioResponse.ok) throw new Error(`HTTP error! status: ${portfolioResponse.status}`);
                const data: PortfolioData = await portfolioResponse.json();
                setPortfolio(data);
                setError(null);
            } catch (e: any) {
                setError(e.message);
            } finally {
                setIsLoading(false);
            }
        };
        fetchPortfolio();
        const interval = setInterval(fetchPortfolio, 2000);
        return () => clearInterval(interval);
    }, []);

    const formatCurrency = (value: number) => (value / 100).toLocaleString('en-US', { style: 'currency', currency: 'USD' });
    const PnlColor = portfolio && portfolio.unrealizedPnl < 0 ? 'text-rose-500' : 'text-emerald-500';

    return (
        <div className="bg-neutral-50 dark:bg-neutral-900 border border-neutral-200 dark:border-neutral-700 rounded-lg flex flex-col">
            <div className="p-4 border-b border-neutral-200 dark:border-neutral-700 flex justify-center items-center">
                <h2 className="text-sm font-semibold text-neutral-700 dark:text-neutral-300">My Portfolio</h2>
            </div>
            <div className="p-5 flex-1 flex flex-col gap-5">
                {isLoading && <div className="text-center text-sm text-neutral-500">Loading...</div>}
                {error && <div className="text-center text-sm text-rose-500">{error}</div>}
                {!portfolio && !isLoading && !error && <div className="text-center text-xs font-mono text-neutral-500 py-4">Portfolio data not available.</div>}
                {portfolio && (
                    <>
                        <div className="grid grid-cols-2 gap-4">
                            <div className="text-center">
                                <label className="text-[10px] text-neutral-500 font-medium uppercase tracking-wider block mb-1">Total Balance</label>
                                <span className="text-xl font-mono text-neutral-800 dark:text-neutral-200">{formatCurrency(portfolio.balance)}</span>
                            </div>
                            <div className="text-center">
                                <label className="text-[10px] text-neutral-500 font-medium uppercase tracking-wider block mb-1">Unrealized P/L</label>
                                <span className={`text-xl font-mono ${PnlColor}`}>{portfolio.unrealizedPnl >= 0 ? '+' : ''}{formatCurrency(portfolio.unrealizedPnl)}</span>
                            </div>
                        </div>
                        <div className="h-px bg-neutral-200 dark:bg-neutral-700 w-full my-1"></div>
                        <div>
                            <label className="text-xs text-neutral-500 font-medium uppercase tracking-wider block mb-2">Positions</label>
                            <div className="space-y-2 max-h-[160px] overflow-y-auto no-scrollbar pr-2">
                                {Object.keys(portfolio.positions).length > 0 ? (
                                    Object.entries(portfolio.positions).map(([symbol, quantity]) => (
                                        <div key={symbol} className="flex justify-between items-center text-sm font-mono p-2 rounded bg-white dark:bg-neutral-950 border border-transparent hover:border-neutral-200 dark:hover:border-neutral-800">
                                            <span className="text-blue-500 dark:text-blue-400 font-bold">{symbol}</span>
                                            <span className={`font-medium ${quantity > 0 ? 'text-emerald-500' : 'text-rose-500'}`}>{quantity.toLocaleString()}</span>
                                        </div>
                                    ))
                                ) : (
                                    <div className="text-center text-xs font-mono text-neutral-500 py-4">No open positions.</div>
                                )}
                            </div>
                        </div>
                        <div className="h-px bg-neutral-200 dark:bg-neutral-700 w-full my-1"></div>
                         <div className="text-xs text-neutral-500 dark:text-neutral-400 space-y-1">
                            <p className="flex justify-between"><span>Available Balance:</span> <span>{portfolio ? formatCurrency(portfolio.balance) : '$0.00'}</span></p>
                        </div>
                    </>
                )}
            </div>
        </div>
    );
};


// Placeholder for the candlestick chart component
const CandlestickChart = () => {
    const { currentSymbol } = useSymbol();
    
    return (
        <div className="bg-neutral-50 dark:bg-neutral-900 border border-neutral-200 dark:border-neutral-700 rounded-lg h-full flex flex-col">
            <div className="p-4 border-b border-neutral-200 dark:border-neutral-700 flex justify-between items-center">
                <h2 className="text-sm font-semibold text-neutral-700 dark:text-neutral-300">{currentSymbol} - 1H Chart</h2>
                {/* Placeholder for time interval selectors */}
                <div className="text-xs text-neutral-500">1H</div>
            </div>
            <div className="flex-1 p-2">
                {/* This is where the lightweight-chart would be rendered */}
                <div className="w-full h-full bg-neutral-100 dark:bg-neutral-950 flex items-center justify-center rounded-md">
                    <p className="text-sm text-neutral-500">Candlestick chart would be here.</p>
                </div>
            </div>
        </div>
    )
}

const ManualTrading: React.FC = () => {
  return (
    <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 h-[calc(100vh-120px)]">
      {/* Left side: Candlestick Chart */}
      <div className="lg:col-span-2">
        <CandlestickChart />
      </div>

      {/* Right side: Control Area */}
      <div className="lg:col-span-1 flex flex-col gap-6 overflow-y-auto no-scrollbar">
        <TradingControls />
        <PortfolioWidget />
      </div>
    </div>
  );
};

export default ManualTrading;
