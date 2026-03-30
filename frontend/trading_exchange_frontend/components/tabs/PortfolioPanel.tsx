// frontend/trading_exchange_frontend/components/tabs/PortfolioPanel.tsx
import React, { useEffect, useState, useCallback } from 'react';
import { PortfolioSnapshot } from '../../types';
import { useApp } from '../../context/AppContext';

type Side = 'buy' | 'sell';
type OrderType = 'limit' | 'market';

const PortfolioPanel: React.FC = () => {
  const { apiBase, userTraderId } = useApp();

  // Portfolio stats
  const [portfolio, setPortfolio] = useState<PortfolioSnapshot | null>(null);

  // Order form
  const [side, setSide] = useState<Side>('buy');
  const [orderType, setOrderType] = useState<OrderType>('limit');
  const [price, setPrice] = useState('');
  const [qty, setQty] = useState('');
  const [submitting, setSubmitting] = useState(false);
  const [feedback, setFeedback] = useState<{ ok: boolean; msg: string } | null>(null);

  const fetchPortfolio = useCallback(() => {
    if (userTraderId === null) return;
    fetch(`${apiBase}/portfolio/${userTraderId}`)
      .then(r => r.json())
      .then((data: PortfolioSnapshot) => setPortfolio(data))
      .catch(() => {});
  }, [apiBase, userTraderId]);

  useEffect(() => {
    fetchPortfolio();
    const id = setInterval(fetchPortfolio, 2000);
    return () => clearInterval(id);
  }, [fetchPortfolio]);

  const submitOrder = async () => {
    if (userTraderId === null) return;
    const qtyNum = parseInt(qty, 10);
    if (!qtyNum || qtyNum <= 0) {
      setFeedback({ ok: false, msg: 'Invalid quantity' });
      return;
    }
    if (orderType === 'limit' && (!price || parseFloat(price) <= 0)) {
      setFeedback({ ok: false, msg: 'Invalid price' });
      return;
    }

    setSubmitting(true);
    setFeedback(null);
    try {
      let res: Response;
      if (orderType === 'limit') {
        res = await fetch(`${apiBase}/orders/limit`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            trader_id: userTraderId,
            is_buy: side === 'buy',
            price: parseFloat(price),
            qty: qtyNum,
            tif: 'GTC',
          }),
        });
      } else {
        res = await fetch(`${apiBase}/orders/market`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            trader_id: userTraderId,
            is_buy: side === 'buy',
            qty: qtyNum,
          }),
        });
      }

      if (res.ok) {
        const data = await res.json();
        setFeedback({ ok: true, msg: `Order #${data.order_id} submitted` });
        setQty('');
        setPrice('');
        fetchPortfolio();
      } else {
        const err = await res.json();
        setFeedback({ ok: false, msg: err.detail ?? 'Order rejected' });
      }
    } catch {
      setFeedback({ ok: false, msg: 'Network error' });
    } finally {
      setSubmitting(false);
      setTimeout(() => setFeedback(null), 4000);
    }
  };

  const fmtUsd = (v: number) =>
    `$${Math.abs(v).toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}`;

  const pnlUsd = portfolio ? portfolio.unrealized_pnl / 10000 : 0;

  return (
    <div className="h-full flex flex-col overflow-y-auto no-scrollbar">

      {/* ── Stats ── */}
      <div className="p-3 border-b border-[#1a1a1a]">
        <p className="text-[9px] text-neutral-600 uppercase tracking-wider mb-2">Portfolio</p>
        {portfolio ? (
          <div className="grid grid-cols-2 gap-x-4 gap-y-2">
            <div>
              <div className="text-[9px] text-neutral-600">Balance</div>
              <div className="text-[12px] text-neutral-200">{fmtUsd(portfolio.balance_usd)}</div>
            </div>
            <div>
              <div className="text-[9px] text-neutral-600">Position</div>
              <div className={`text-[12px] ${portfolio.position > 0 ? 'text-green-400' : portfolio.position < 0 ? 'text-red-400' : 'text-neutral-400'}`}>
                {portfolio.position > 0 ? '+' : ''}{portfolio.position} BTC
              </div>
            </div>
            <div>
              <div className="text-[9px] text-neutral-600">Unrealized PnL</div>
              <div className={`text-[12px] ${pnlUsd >= 0 ? 'text-green-400' : 'text-red-400'}`}>
                {pnlUsd >= 0 ? '+' : '-'}{fmtUsd(pnlUsd)}
              </div>
            </div>
            <div>
              <div className="text-[9px] text-neutral-600">Avg Cost</div>
              <div className="text-[12px] text-neutral-400">
                {portfolio.avg_cost > 0 ? fmtUsd(portfolio.avg_cost) : '—'}
              </div>
            </div>
          </div>
        ) : (
          <div className="text-neutral-600 text-xs">Loading…</div>
        )}
      </div>

      {/* ── Order Entry ── */}
      <div className="p-3 flex flex-col gap-3">
        <p className="text-[9px] text-neutral-600 uppercase tracking-wider">New Order</p>

        {/* Side toggle */}
        <div className="flex rounded overflow-hidden border border-[#2a2a2a]">
          <button
            onClick={() => setSide('buy')}
            className={`flex-1 py-1.5 text-[11px] transition-colors
              ${side === 'buy' ? 'bg-green-500/20 text-green-400' : 'text-neutral-500 hover:text-neutral-300'}`}
          >
            BUY
          </button>
          <button
            onClick={() => setSide('sell')}
            className={`flex-1 py-1.5 text-[11px] transition-colors border-l border-[#2a2a2a]
              ${side === 'sell' ? 'bg-red-500/20 text-red-400' : 'text-neutral-500 hover:text-neutral-300'}`}
          >
            SELL
          </button>
        </div>

        {/* Order type toggle */}
        <div className="flex rounded overflow-hidden border border-[#2a2a2a]">
          <button
            onClick={() => setOrderType('limit')}
            className={`flex-1 py-1.5 text-[11px] transition-colors
              ${orderType === 'limit' ? 'bg-neutral-700 text-neutral-200' : 'text-neutral-500 hover:text-neutral-300'}`}
          >
            Limit
          </button>
          <button
            onClick={() => setOrderType('market')}
            className={`flex-1 py-1.5 text-[11px] transition-colors border-l border-[#2a2a2a]
              ${orderType === 'market' ? 'bg-neutral-700 text-neutral-200' : 'text-neutral-500 hover:text-neutral-300'}`}
          >
            Market
          </button>
        </div>

        {/* Price input (limit only) */}
        {orderType === 'limit' && (
          <div>
            <label className="text-[9px] text-neutral-600 uppercase tracking-wider block mb-1">Price (USD)</label>
            <input
              type="number"
              value={price}
              onChange={e => setPrice(e.target.value)}
              placeholder="64200.00"
              className="w-full bg-[#111] border border-[#2a2a2a] rounded px-2 py-1.5 text-[11px] text-neutral-200 placeholder-neutral-700 focus:outline-none focus:border-neutral-500"
            />
          </div>
        )}

        {/* Qty input */}
        <div>
          <label className="text-[9px] text-neutral-600 uppercase tracking-wider block mb-1">Quantity (BTC)</label>
          <input
            type="number"
            value={qty}
            onChange={e => setQty(e.target.value)}
            placeholder="1"
            min="1"
            step="1"
            className="w-full bg-[#111] border border-[#2a2a2a] rounded px-2 py-1.5 text-[11px] text-neutral-200 placeholder-neutral-700 focus:outline-none focus:border-neutral-500"
          />
        </div>

        {/* Submit */}
        <button
          onClick={submitOrder}
          disabled={submitting || userTraderId === null}
          className={`w-full py-2 rounded text-[12px] font-medium transition-colors
            ${side === 'buy'
              ? 'bg-green-500/20 text-green-400 hover:bg-green-500/30 border border-green-500/40'
              : 'bg-red-500/20 text-red-400 hover:bg-red-500/30 border border-red-500/40'}
            ${(submitting || userTraderId === null) ? 'opacity-50 cursor-not-allowed' : 'cursor-pointer'}`}
        >
          {submitting ? 'Submitting…' : `${side === 'buy' ? 'Buy' : 'Sell'} BTC`}
        </button>

        {/* Feedback */}
        {feedback && (
          <p className={`text-[10px] text-center ${feedback.ok ? 'text-green-400' : 'text-red-400'}`}>
            {feedback.msg}
          </p>
        )}
      </div>
    </div>
  );
};

export default PortfolioPanel;
