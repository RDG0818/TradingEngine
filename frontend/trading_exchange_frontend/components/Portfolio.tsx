import React, { useState, useEffect } from 'react';
import { Briefcase } from 'lucide-react';
import { PortfolioData } from '../types';

const Portfolio: React.FC = () => {
    const [portfolio, setPortfolio] = useState<PortfolioData | null>(null);
    const [isLoading, setIsLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);

    useEffect(() => {
        const fetchPortfolio = async () => {
            try {
                // First get the default trader ID
                const idResponse = await fetch('http://localhost:8000/trader/default_id');
                if (!idResponse.ok) {
                    throw new Error('Failed to fetch default trader ID');
                }
                const { traderId } = await idResponse.json();

                if (traderId === null || traderId === undefined) {
                    // Silently fail if no trader ID is available yet, will retry
                    return;
                }

                // Then fetch portfolio for that trader
                const portfolioResponse = await fetch(`http://localhost:8000/portfolio/${traderId}`);
                if (!portfolioResponse.ok) {
                    throw new Error(`HTTP error! status: ${portfolioResponse.status}`);
                }
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

        // Fetch immediately and then set an interval
        fetchPortfolio();
        const interval = setInterval(fetchPortfolio, 2000); // refresh every 2s

        return () => clearInterval(interval);
    }, []);

    const formatCurrency = (value: number) => {
        return (value / 100).toLocaleString('en-US', {
            style: 'currency',
            currency: 'USD',
        });
    };

    const PnlColor = portfolio && portfolio.unrealizedPnl < 0 ? 'text-rose-500' : 'text-emerald-500';

    return (
        <div className="bg-neutral-50 dark:bg-neutral-900 border border-neutral-200 dark:border-neutral-700 rounded-lg p-1 h-full flex flex-col">
            <div className="p-4 border-b border-neutral-200 dark:border-neutral-700 flex justify-center items-center">
                <h2 className="text-sm font-semibold text-neutral-700 dark:text-neutral-300">My Portfolio</h2>
            </div>

            <div className="p-5 flex-1 flex flex-col gap-5">
                {isLoading && <div className="text-center text-sm text-neutral-500">Loading portfolio...</div>}
                {error && !isLoading && <div className="text-center text-sm text-rose-500">{error}</div>}
                
                {!portfolio && !isLoading && !error && (
                     <div className="text-center text-xs font-mono text-neutral-500 py-4">
                        Portfolio data not available.
                    </div>
                )}

                {portfolio && (
                    <>
                        {/* Key metrics */}
                        <div className="grid grid-cols-2 gap-4">
                            <div className="text-center">
                                <label className="text-[10px] text-neutral-500 font-medium uppercase tracking-wider block mb-1">Total Balance</label>
                                <span className="text-xl font-mono text-neutral-800 dark:text-neutral-200">{formatCurrency(portfolio.balance)}</span>
                            </div>
                            <div className="text-center">
                                <label className="text-[10px] text-neutral-500 font-medium uppercase tracking-wider block mb-1">Unrealized P/L</label>
                                <span className={`text-xl font-mono ${PnlColor}`}>
                                    {portfolio.unrealizedPnl >= 0 ? '+' : ''}{formatCurrency(portfolio.unrealizedPnl)}
                                </span>
                            </div>
                        </div>

                        <div className="h-px bg-neutral-200 dark:bg-neutral-700 w-full my-1"></div>

                        {/* Positions */}
                        <div>
                            <label className="text-xs text-neutral-500 font-medium uppercase tracking-wider block mb-2">Positions</label>
                            <div className="space-y-2 max-h-[160px] overflow-y-auto no-scrollbar pr-2">
                                {Object.keys(portfolio.positions).length > 0 ? (
                                    Object.entries(portfolio.positions).map(([symbol, quantity]) => (
                                        <div key={symbol} className="flex justify-between items-center text-sm font-mono p-2 rounded bg-white dark:bg-neutral-950 border border-transparent hover:border-neutral-200 dark:hover:border-neutral-800">
                                            <span className="text-blue-500 dark:text-blue-400 font-bold">{symbol}</span>
                                            <span className={`font-medium ${quantity > 0 ? 'text-emerald-500' : 'text-rose-500'}`}>
                                                {quantity.toLocaleString()}
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
                    </>
                )}
            </div>
        </div>
    );
};

export default Portfolio;
