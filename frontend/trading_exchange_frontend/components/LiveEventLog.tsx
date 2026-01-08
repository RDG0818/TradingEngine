import React from 'react';

const LiveEventLog: React.FC = () => {

  // Placeholder data
  const events = [
    { time: '12:34:56.789', message: 'Order Filled: BUY 1.5 ETH @ 3012.50' },
    { time: '12:34:56.543', message: 'Order Submitted: SELL 0.5 BTC @ 60123.45' },
    { time: '12:34:55.999', message: 'Liquidity bot added 10 orders to the book.' },
    { time: '12:34:55.123', message: 'Order Filled: SELL 2.0 ETH @ 3012.00' },
    { time: '12:34:54.876', message: 'Order Canceled: #12345678' },
    { time: '12:34:54.321', message: 'Order Submitted: BUY 1.0 ETH @ 3011.50' },
    { time: '12:34:53.987', message: 'Market data feed for ETH-USD-PERP reconnected.' },
    { time: '12:34:53.001', message: 'Order Filled: BUY 0.1 BTC @ 60120.00' },
    { time: '12:34:52.555', message: 'System latency normal: 0.25ms RTT' },
    { time: '12:34:52.111', message: 'Automated trader executed strategy "ScalperV2".' },
  ];

  return (
    <div className="bg-neutral-50 dark:bg-neutral-900 border border-neutral-200 dark:border-neutral-700 rounded-lg p-1 h-full flex flex-col">
      <div className="p-4 border-b border-neutral-200 dark:border-neutral-700">
        <h2 className="text-sm font-semibold text-neutral-700 dark:text-neutral-300">Live Event Log</h2>
      </div>
      <div className="p-4 flex-1 overflow-y-auto no-scrollbar">
        <ul className="flex flex-col gap-2">
          {events.map((event, index) => (
            <li key={index} className="flex items-start gap-3">
              <span className="text-xs font-mono text-neutral-500 dark:text-neutral-600 pt-0.5">{event.time}</span>
              <p className="text-xs text-neutral-700 dark:text-neutral-300 font-mono">{event.message}</p>
            </li>
          ))}
        </ul>
      </div>
    </div>
  );
};

export default LiveEventLog;
