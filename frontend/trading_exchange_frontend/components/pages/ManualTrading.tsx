import React, { useState, useEffect } from 'react';
import { useSymbol } from '../../context/SymbolContext';
import { PortfolioData } from '../../types';

type OrderType = 'Market' | 'Limit' | 'Stop Market';

const TradingInput: React.FC<{ label: string; placeholder: string; type?: string; }> = (props) => (
    <div>
        <label className="block text-xs text-neutral-400 mb-1">{props.label}</label>
        <input 
            type={props.type || "number"} 
            placeholder={props.placeholder} 
            className="w-full mt-1 p-2 rounded-md bg-neutral-800 border border-neutral-700/50 focus:ring-1 focus:ring-blue-500 outline-none font-mono"
        />
    </div>
);

const TradingControls: React.FC = () => {
    const { currentSymbol } = useSymbol();
    const [side, setSide] = useState<'buy' | 'sell'>('buy');
    const [orderType, setOrderType] = useState<OrderType>('Limit');

    const renderInputs = () => {
        switch (orderType) {
            case 'Market':
                return <TradingInput label={`Amount (${currentSymbol.split('-')[0]})`} placeholder="0.5" />;
            case 'Limit':
                return (
                    <>
                        <TradingInput label="Price (USD)" placeholder="60,000.00" />
                        <TradingInput label={`Amount (${currentSymbol.split('-')[0]})`} placeholder="0.5" />
                    </>
                );
            case 'Stop Market':
                return (
                     <>
                        <TradingInput label="Stop Price (USD)" placeholder="59,500.00" />
                        <TradingInput label={`Amount (${currentSymbol.split('-')[0]})`} placeholder="0.5" />
                    </>
                );
        }
    }
  
    return (
      <div className="bg-neutral-950/70 border border-white/5 rounded-lg flex flex-col">
        <div className="p-3 border-b border-white/5">
          <h2 className="text-sm font-semibold text-neutral-300 text-center">Manual Order</h2>
        </div>
        <div className="p-4 flex-1">
            <div className="grid grid-cols-2 gap-1 bg-neutral-800 rounded-md p-1 mb-4">
                <button 
                    onClick={() => setSide('buy')}
                    className={`py-2 text-sm font-semibold rounded-[5px] transition-colors ${side === 'buy' ? 'bg-emerald-500/80 text-white' : 'text-neutral-300 hover:bg-neutral-700/50'}`}
                >
                    Buy
                </button>
                <button 
                    onClick={() => setSide('sell')}
                    className={`py-2 text-sm font-semibold rounded-[5px] transition-colors ${side === 'sell' ? 'bg-rose-500/80 text-white' : 'text-neutral-300 hover:bg-neutral-700/50'}`}
                >
                    Sell
                </button>
            </div>

            <div className="grid grid-cols-3 gap-1 text-xs mb-4 text-center">
                {(['Limit', 'Market', 'Stop Market'] as OrderType[]).map(type => (
                    <button key={type} onClick={() => setOrderType(type)} className={`py-1 rounded transition-colors ${orderType === type ? 'text-blue-400 font-semibold' : 'text-neutral-500 hover:text-blue-400/70'}`}>
                        {type}
                    </button>
                ))}
            </div>

            <div className="space-y-3">
                {renderInputs()}
                <button className={`w-full py-3 rounded-md text-white font-semibold transition-colors ${side === 'buy' ? 'bg-emerald-600 hover:bg-emerald-700' : 'bg-rose-600 hover:bg-rose-700'}`}>
                    {side === 'buy' ? 'Buy' : 'Sell'} {currentSymbol.split('-')[0]}
                </button>
            </div>
        </div>
      </div>
    );
};

const PortfolioWidget: React.FC = () => {
    const [portfolio, setPortfolio] = useState<PortfolioData | null>(null);

    useEffect(() => {
        const fetchPortfolio = async () => {
            try {
                const idResponse = await fetch('http://localhost:8000/trader/default_id');
                if (!idResponse.ok) return;
                const { traderId } = await idResponse.json();
                if (traderId === null || traderId === undefined) return;
                const portfolioResponse = await fetch(`http://localhost:8000/portfolio/${traderId}`);
                if (!portfolioResponse.ok) return;
                const data: PortfolioData = await portfolioResponse.json();
                setPortfolio(data);
            } catch (e) { console.error(e); }
        };
        fetchPortfolio();
        const interval = setInterval(fetchPortfolio, 2000);
        return () => clearInterval(interval);
    }, []);

    const formatCurrency = (value: number) => (value / 100).toLocaleString('en-US', { style: 'currency', currency: 'USD' });
    const pnlColor = portfolio && portfolio.unrealizedPnl < 0 ? 'text-rose-500' : 'text-emerald-500';

    return (
        <div className="bg-neutral-950/70 border border-white/5 rounded-lg flex flex-col">
            <div className="p-3 border-b border-white/5 flex justify-center items-center">
                <h2 className="text-sm font-semibold text-neutral-300">My Portfolio</h2>
            </div>
            {portfolio ? (
                <div className="p-4 flex-1 flex flex-col gap-4">
                    <div className="grid grid-cols-2 gap-4 text-center">
                        <div>
                            <label className="text-[10px] text-neutral-500 font-medium uppercase tracking-wider block mb-1">Balance</label>
                            <span className="text-lg font-mono text-neutral-200">{formatCurrency(portfolio.balance)}</span>
                        </div>
                        <div>
                            <label className="text-[10px] text-neutral-500 font-medium uppercase tracking-wider block mb-1">Unrealized P/L</label>
                            <span className={`text-lg font-mono ${pnlColor}`}>{portfolio.unrealizedPnl >= 0 ? '+' : ''}{formatCurrency(portfolio.unrealizedPnl)}</span>
                        </div>
                    </div>
                    <div className="h-px bg-white/5 w-full"></div>
                    <div>
                        <label className="text-xs text-neutral-400 font-medium uppercase tracking-wider block mb-2 px-1">Positions</label>
                        <div className="space-y-1 max-h-[120px] overflow-y-auto no-scrollbar">
                            {Object.keys(portfolio.positions).length > 0 ? (
                                Object.entries(portfolio.positions).map(([symbol, quantity]) => (
                                    <div key={symbol} className="flex justify-between items-center text-sm font-mono p-2 rounded hover:bg-white/5">
                                        <span className="text-blue-400 font-bold">{symbol}</span>
                                        <span className={`font-medium ${quantity > 0 ? 'text-emerald-500' : 'text-rose-500'}`}>{quantity.toLocaleString()}</span>
                                    </div>
                                ))
                            ) : (
                                <div className="text-center text-xs font-mono text-neutral-500 py-4">No open positions.</div>
                            )}
                        </div>
                    </div>
                </div>
            ) : (
                <div className="p-4 flex-1 flex items-center justify-center text-xs font-mono text-neutral-500">
                    Portfolio data not available.
                </div>
            )}
        </div>
    );
};

const CandlestickChart = () => {
    const { currentSymbol } = useSymbol();
    
    return (
        <div className="bg-neutral-950/70 border border-white/5 rounded-lg h-full flex flex-col">
            <div className="p-3 border-b border-white/5 flex justify-between items-center">
                <h2 className="text-sm font-semibold text-neutral-300">{currentSymbol} - Chart</h2>
                <div className="text-xs text-neutral-500 font-mono flex gap-2">
                    <button className="text-blue-400">1H</button>
                    <button className="hover:text-white">1D</button>
                    <button className="hover:text-white">1W</button>
                </div>
            </div>
            <div className="flex-1 p-2">
                <div className="w-full h-full chart-grid-bg flex items-center justify-center rounded-md">
                    <p className="text-sm text-neutral-500">Candlestick chart would be rendered here.</p>
                </div>
            </div>
        </div>
    )
}

const ManualTrading: React.FC = () => {
  return (
    <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 h-[calc(100vh-120px)] p-4 lg:p-8">
      <div className="lg:col-span-2">
        <CandlestickChart />
      </div>
      <div className="lg:col-span-1 flex flex-col gap-6 overflow-y-auto no-scrollbar">
        <TradingControls />
        <PortfolioWidget />
      </div>
    </div>
  );
};

export default ManualTrading;
