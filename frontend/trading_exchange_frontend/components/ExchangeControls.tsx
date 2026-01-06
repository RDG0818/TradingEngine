import React, { useState } from 'react';
import { Play, Square, Settings2, Cpu } from 'lucide-react';
import { ExchangeMode } from '../types';

const ExchangeControls: React.FC = () => {
  const [isRunning, setIsRunning] = useState(false);
  const [mode, setMode] = useState<ExchangeMode>('Simulation');
  const [autoTrader, setAutoTrader] = useState(true);

  return (
    <div className="bg-neutral-900 border border-neutral-800 rounded-lg p-1 h-full flex flex-col">
      <div className="p-4 border-b border-neutral-800 flex justify-between items-center">
        <h2 className="text-sm font-semibold text-neutral-300">Exchange Controls</h2>
        <Settings2 size={16} className="text-neutral-600" />
      </div>

      <div className="p-6 flex-1 flex flex-col gap-8">
        
        {/* Main Start/Stop */}
        <div>
          <button
            onClick={() => setIsRunning(!isRunning)}
            className={`w-full group relative flex items-center justify-center gap-3 py-4 rounded font-medium transition-all duration-200 ${
              isRunning 
                ? 'bg-rose-900/20 text-rose-500 border border-rose-900/50 hover:bg-rose-900/30' 
                : 'bg-emerald-900/20 text-emerald-500 border border-emerald-900/50 hover:bg-emerald-900/30'
            }`}
          >
             {isRunning ? <Square size={18} fill="currentColor" /> : <Play size={18} fill="currentColor" />}
             {isRunning ? 'STOP EXCHANGE' : 'START EXCHANGE'}
             
             <span className={`absolute right-4 flex h-2 w-2`}>
                <span className={`${isRunning ? 'animate-ping bg-rose-500' : 'bg-emerald-500'} absolute inline-flex h-full w-full rounded-full opacity-75`}></span>
                <span className={`${isRunning ? 'bg-rose-500' : 'bg-emerald-500'} relative inline-flex rounded-full h-2 w-2`}></span>
             </span>
          </button>
          <div className="mt-3 flex justify-between items-center px-1">
             <span className="text-xs text-neutral-500 font-mono uppercase">Status</span>
             <span className={`text-xs font-mono font-medium ${isRunning ? 'text-emerald-500' : 'text-neutral-500'}`}>
                {isRunning ? 'ENGINE RUNNING - MATCHING ACTIVE' : 'ENGINE IDLE'}
             </span>
          </div>
        </div>

        <div className="h-px bg-neutral-800 w-full"></div>

        {/* Configuration */}
        <div className="space-y-6">
            
            {/* Mode Selector */}
            <div className="flex flex-col gap-2">
                <label className="text-xs text-neutral-400 font-medium uppercase tracking-wider">Operational Mode</label>
                <div className="relative">
                    <select 
                        value={mode}
                        onChange={(e) => setMode(e.target.value as ExchangeMode)}
                        className="w-full bg-neutral-950 border border-neutral-800 text-neutral-300 text-sm rounded px-3 py-2.5 focus:outline-none focus:border-neutral-600 appearance-none font-mono"
                    >
                        <option value="Simulation">Simulation Mode</option>
                        <option value="Manual Trading">Manual Trading</option>
                    </select>
                    <div className="absolute right-3 top-3 pointer-events-none">
                        <svg className="w-4 h-4 text-neutral-600" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M19 9l-7 7-7-7"></path></svg>
                    </div>
                </div>
            </div>

            {/* Auto Trader Toggle */}
            <div className="flex items-center justify-between p-3 bg-neutral-950 rounded border border-neutral-800">
                <div className="flex items-center gap-3">
                    <Cpu size={18} className="text-neutral-500" />
                    <div>
                        <span className="block text-sm text-neutral-300 font-medium">Automated Traders</span>
                        <span className="block text-[10px] text-neutral-600 uppercase">Liquidity Bot Swarm</span>
                    </div>
                </div>
                <button 
                    onClick={() => setAutoTrader(!autoTrader)}
                    className={`relative inline-flex h-5 w-9 items-center rounded-full transition-colors focus:outline-none ${autoTrader ? 'bg-blue-900/50 border border-blue-800' : 'bg-neutral-800 border border-neutral-700'}`}
                >
                    <span
                        className={`${
                        autoTrader ? 'translate-x-5 bg-blue-400' : 'translate-x-1 bg-neutral-500'
                        } inline-block h-3 w-3 transform rounded-full transition-transform duration-200`}
                    />
                </button>
            </div>
        </div>

      </div>
    </div>
  );
};

export default ExchangeControls;