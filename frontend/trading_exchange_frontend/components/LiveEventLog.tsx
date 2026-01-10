import React from 'react';

const LiveEventLog: React.FC = () => {

  // Placeholder data
  const events = [
    { time: '12:34:56.789', type: 'FILL', message: 'BUY 1.5 ETH @ 3012.50' },
    { time: '12:34:56.543', type: 'NEW', message: 'SELL 0.5 BTC @ 60123.45' },
    { time: '12:34:55.999', type: 'INFO', message: 'Liquidity bot added 10 orders.' },
    { time: '12:34:55.123', type: 'FILL', message: 'SELL 2.0 ETH @ 3012.00' },
    { time: '12:34:54.876', type: 'CANCEL', message: 'Order #12345678' },
    { time: '12:34:54.321', type: 'NEW', message: 'BUY 1.0 ETH @ 3011.50' },
    { time: '12:34:53.987', type: 'INFO', message: 'ETH-USD-PERP reconnected.' },
    { time: '12:34:53.001', type: 'FILL', message: 'BUY 0.1 BTC @ 60120.00' },
    { time: '12:34:52.555', type: 'SYSTEM', message: 'System latency normal: 0.25ms' },
    { time: '12:34:52.111', type: 'EXEC', message: 'Strategy "ScalperV2" triggered.' },
    { time: '12:34:51.888', type: 'ERROR', message: 'Failed to place order: Insufficient funds.' },
  ];

  const typeColor: Record<string, string> = {
    'FILL': 'text-emerald-400',
    'NEW': 'text-blue-400',
    'CANCEL': 'text-amber-400',
    'INFO': 'text-neutral-500',
    'SYSTEM': 'text-purple-400',
    'EXEC': 'text-cyan-400',
    'ERROR': 'text-rose-500',
  }

  return (
    <div className="bg-neutral-950/70 border border-white/5 rounded-lg h-full flex flex-col">
      <div className="p-3 border-b border-white/5 flex justify-center items-center">
        <h2 className="text-sm font-semibold text-neutral-300">Live Event Log</h2>
      </div>
      <div className="p-4 flex-1 overflow-y-auto no-scrollbar">
        <div className="font-mono text-xs">
          {events.map((event, index) => (
            <div key={index} className="grid grid-cols-[auto_50px_1fr] items-start gap-3 hover:bg-white/5 rounded px-1 py-0.5">
              <span className="text-neutral-600">{event.time}</span>
              <span className={`${typeColor[event.type] || 'text-neutral-400'} font-bold`}>[{event.type}]</span>
              <span className="text-neutral-300">{event.message}</span>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
};

export default LiveEventLog;
