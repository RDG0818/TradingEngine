import React from 'react';
import { ArrowRight, Terminal, MonitorPlay } from 'lucide-react';

const ActionCenter: React.FC = () => {
  return (
    <div className="flex flex-col items-center justify-center py-8 gap-4">
        <button className="group bg-neutral-100 text-neutral-950 px-8 py-3 rounded font-semibold text-sm hover:bg-white hover:scale-[1.01] transition-all flex items-center gap-2 shadow-[0_0_15px_rgba(255,255,255,0.1)]">
            Enter Manual Trading
            <ArrowRight size={16} className="group-hover:translate-x-1 transition-transform" />
        </button>
        
        <div className="flex items-center gap-6">
            <button className="flex items-center gap-2 text-xs text-neutral-500 hover:text-neutral-300 transition-colors">
                <MonitorPlay size={12} />
                View Market Simulation
            </button>
            <div className="w-px h-3 bg-neutral-800"></div>
            <button className="flex items-center gap-2 text-xs text-neutral-500 hover:text-neutral-300 transition-colors">
                <Terminal size={12} />
                Open Strategy Lab
            </button>
        </div>
    </div>
  );
};

export default ActionCenter;