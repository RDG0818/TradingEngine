import React, { useState, useEffect } from 'react';
import { Server } from 'lucide-react';

const Header: React.FC = () => {
  const [time, setTime] = useState(new Date());

  useEffect(() => {
    const timerId = setInterval(() => setTime(new Date()), 1000);
    return () => clearInterval(timerId);
  }, []);

  const formatTime = (date: Date) => {
    const hours = date.getHours().toString().padStart(2, '0');
    const minutes = date.getMinutes().toString().padStart(2, '0');
    const seconds = date.getSeconds().toString().padStart(2, '0');
    return `${hours}:${minutes}:${seconds} UTC`;
  };

  return (
    <header className="h-14 border-b border-neutral-800 bg-neutral-950/80 backdrop-blur-sm flex items-center justify-between px-6 sticky top-0 z-10">
      <div className="flex items-center gap-4">
        <div className="text-sm font-mono text-neutral-500">{formatTime(time)}</div>
      </div>

      <div className="flex items-center gap-6">
        <div className="flex items-center gap-2 px-3 py-1 rounded-full bg-neutral-900 border border-neutral-800">
          <Server size={12} className="text-neutral-500" />
          <span className="text-xs font-mono text-neutral-400">ENV: LOCAL_Sim</span>
        </div>

        <div className="flex items-center gap-2">
           <div className="relative flex h-2 w-2">
              <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-emerald-400 opacity-75"></span>
              <span className="relative inline-flex rounded-full h-2 w-2 bg-emerald-500"></span>
            </div>
            <span className="text-xs font-medium text-emerald-500 tracking-wide">LIVE – SIMULATION MODE</span>
        </div>
      </div>
    </header>
  );
};

export default Header;