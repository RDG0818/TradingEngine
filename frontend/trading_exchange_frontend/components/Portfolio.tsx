import React, { useState, useEffect } from 'react';
import { PortfolioData } from '../types';

const Portfolio: React.FC = () => {
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
                console.error("Error fetching portfolio:", e);
            } finally {
                setIsLoading(false);
            }
        };

        fetchPortfolio();
        const interval = setInterval(fetchPortfolio, 2000);
        return () => clearInterval(interval);
    }, []);

    const formatCurrency = (value: number) => (value / 100).toLocaleString('en-US', { style: 'currency', currency: 'USD' });

    const pnlColor = portfolio && portfolio.unrealizedPnl < 0 ? 'text-rose-500' : 'text-emerald-500';

    const renderContent = () => {
        if (isLoading) return <div className="text-center text-sm text-neutral-500 p-4">Loading portfolio...</div>;
        if (error) return <div className="text-center text-sm text-rose-500 p-4">{error}</div>;
        if (!portfolio) return (
            <div className="text-center text-xs font-mono text-neutral-500 py-4">
                Portfolio data not available.
            </div>
        );

        return (
            <div className="p-4 flex-1 flex flex-col gap-4">
                {/* Key metrics */}
                <div className="grid grid-cols-2 gap-4 text-center">
                    <div>
                        <label className="text-[10px] text-neutral-500 font-medium uppercase tracking-wider block mb-1">Total Balance</label>
                        <span className="text-xl font-mono text-neutral-200">{formatCurrency(portfolio.balance)}</span>
                    </div>
                    <div>
                        <label className="text-[10px] text-neutral-500 font-medium uppercase tracking-wider block mb-1">Unrealized P/L</label>
                        <span className={`text-xl font-mono ${pnlColor}`}>
                            {portfolio.unrealizedPnl >= 0 ? '+' : ''}{formatCurrency(portfolio.unrealizedPnl)}
                        </span>
                    </div>
                </div>

                <div className="h-px bg-white/5 w-full"></div>

                {/* Positions */}
                <div>
                    <label className="text-xs text-neutral-400 font-medium uppercase tracking-wider block mb-2 px-1">Positions</label>
                    <div className="space-y-1 max-h-[160px] overflow-y-auto no-scrollbar">
                        {Object.keys(portfolio.positions).length > 0 ? (
                            Object.entries(portfolio.positions).map(([symbol, quantity]) => (
                                <div key={symbol} className="flex justify-between items-center text-sm font-mono p-2 rounded hover:bg-white/5">
                                    <span className="text-blue-400 font-bold">{symbol}</span>
                                    <span className={`font-medium ${quantity > 0 ? 'text-emerald-500' : 'text-rose-500'}`}>
                                        {quantity > 0 ? '+' : ''}{quantity.toLocaleString()}
                                    </span>
                                </div>
                            ))
                        ) : (
                            <div className="text-center text-xs font-mono text-neutral-500 py-4">
                                No open positions.
                            </div>
                        )}
                    </div>
                </div>
            </div>
        );
    };

    return (
        <div className="bg-neutral-950/70 border border-white/5 rounded-lg h-full flex flex-col">
            <div className="p-3 border-b border-white/5 flex justify-center items-center">
                <h2 className="text-sm font-semibold text-neutral-300">My Portfolio</h2>
            </div>
            {renderContent()}
        </div>
    );
};

export default Portfolio;
