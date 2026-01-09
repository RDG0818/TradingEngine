import React, { useState, useEffect } from 'react';
import { Server, Play, Square, Loader } from 'lucide-react';

const Header: React.FC = () => {
    const [isRunning, setIsRunning] = useState(false);
    const [isToggling, setIsToggling] = useState(false);
    const [tickInterval, setTickInterval] = useState(10); // Default to 10ms
  
    // Fetch initial engine status on component mount
    useEffect(() => {
      const fetchStatus = async () => {
        try {
          const response = await fetch('http://localhost:8000/engine/status');
          if (response.ok) {
            const data = await response.json();
            setIsRunning(data.isRunning);
          }
        } catch (error) {
          console.error('Failed to fetch engine status:', error);
        }
      };
      fetchStatus();
    }, []);
  
    const handleToggleExchange = async () => {
      setIsToggling(true);
      const endpoint = isRunning ? '/engine/stop' : '/engine/start';
      let success = false;
      try {
        const response = await fetch(`http://localhost:8000${endpoint}`, {
          method: 'POST',
        });
        if (response.ok) {
          success = true;
        } else {
          console.error('Failed to toggle engine state');
        }
      } catch (error) {
        console.error('Error toggling engine state:', error);
      } finally {
        setTimeout(() => {
          if (success) {
            setIsRunning(prevIsRunning => !prevIsRunning);
          }
          setIsToggling(false);
        }, 750); // Simulate action time
      }
    };
  
    const handleIntervalChange = (interval: number) => {
      setTickInterval(interval);
      // TODO: Implement API call to backend when the endpoint is ready
    };
  
    const getButtonContent = () => {
      if (isToggling) {
        return (
          <>
            <Loader size={16} className="animate-spin" />
            <span className="text-xs font-medium tracking-wider">
                {isRunning ? 'SHUTTING DOWN' : 'SPINNING UP'}
            </span>
          </>
        );
      }
      return (
        <>
          {isRunning ? <Square size={14} fill="currentColor" /> : <Play size={14} fill="currentColor" />}
          <span className="text-xs font-medium tracking-wider">
            {isRunning ? 'STOP EXCHANGE' : 'START EXCHANGE'}
          </span>
        </>
      );
    };

  return (
    <header className="h-14 border-b border-neutral-200 dark:border-neutral-800 bg-white/80 dark:bg-neutral-950/80 backdrop-blur-sm flex items-center justify-between px-6 sticky top-0 z-10">
      <div className="flex items-center gap-2">
           <div className="relative flex h-2 w-2">
              <span className={`animate-ping absolute inline-flex h-full w-full rounded-full opacity-75 ${isRunning ? 'bg-emerald-400' : 'bg-rose-400'}`}></span>
              <span className={`relative inline-flex rounded-full h-2 w-2 ${isRunning ? 'bg-emerald-500' : 'bg-rose-500'}`}></span>
            </div>
            <span className={`text-xs font-medium tracking-wide ${isRunning ? 'text-emerald-600 dark:text-emerald-500' : 'text-rose-600 dark:text-rose-500'}`}>
                {isToggling ? 'TRANSITIONING...' : (isRunning ? 'LIVE – SIMULATION MODE' : 'ENGINE IDLE')}
            </span>
        </div>

      <div className="flex items-center gap-6">
        {/* Market Tick Interval */}
        <div className="flex items-center gap-2">
            <label className="text-xs text-neutral-500 dark:text-neutral-400 font-medium uppercase tracking-wider">Tick Interval</label>
            <div className="flex items-center gap-1 bg-neutral-100 dark:bg-neutral-900 p-0.5 rounded-md border border-neutral-200 dark:border-neutral-800">
                {[1, 5, 10, 50, 100].map(interval => (
                    <button
                        key={interval}
                        onClick={() => handleIntervalChange(interval)}
                        className={`px-2 py-0.5 text-xs font-mono rounded-[4px] ${
                            tickInterval === interval
                                ? 'bg-blue-500 text-white'
                                : 'text-neutral-600 dark:text-neutral-400 hover:bg-neutral-200 dark:hover:bg-neutral-800'
                        }`}
                    >
                        {interval}ms
                    </button>
                ))}
            </div>
        </div>
        
        {/* Main Start/Stop */}
        <button
            onClick={handleToggleExchange}
            disabled={isToggling}
            className={`flex items-center justify-center gap-2 px-4 py-2 rounded-md font-sans transition-all duration-200 h-9 ${
            isRunning 
                ? 'bg-rose-100 text-rose-600 border border-rose-200 hover:bg-rose-200/70 dark:bg-rose-900/20 dark:text-rose-500 dark:border-rose-900/50 dark:hover:bg-rose-900/30' 
                : 'bg-emerald-100 text-emerald-600 border border-emerald-200 hover:bg-emerald-200/70 dark:bg-emerald-900/20 dark:text-emerald-500 dark:border-emerald-900/50 dark:hover:bg-emerald-900/30'
            } ${isToggling && 'opacity-70 cursor-not-allowed'}`}
        >
            {getButtonContent()}
        </button>
      </div>
    </header>
  );
};

export default Header;
