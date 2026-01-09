import React, { useState, useEffect } from 'react';
import { Play, Square, Settings2, Cpu, Loader } from 'lucide-react';

const ExchangeControls: React.FC = () => {
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
    // try {
    //     await fetch('http://localhost:8000/engine/tick_interval', {
    //         method: 'POST',
    //         headers: { 'Content-Type': 'application/json' },
    //         body: JSON.stringify({ interval_ms: interval }),
    //     });
    // } catch (error) {
    //     console.error('Failed to set tick interval:', error);
    // }
  };

  const getButtonContent = () => {
    if (isToggling) {
      return (
        <>
          <Loader size={18} className="animate-spin" />
          {isRunning ? 'SHUTTING DOWN...' : 'SPINNING UP...'}
        </>
      );
    }
    return (
      <>
        {isRunning ? <Square size={18} fill="currentColor" /> : <Play size={18} fill="currentColor" />}
        {isRunning ? 'STOP EXCHANGE' : 'START EXCHANGE'}
      </>
    );
  }

  return (
    <div className="bg-neutral-50 dark:bg-neutral-900 border border-neutral-200 dark:border-neutral-700 rounded-lg p-1 h-full flex flex-col">
      <div className="p-4 border-b border-neutral-200 dark:border-neutral-700 flex justify-between items-center">
        <h2 className="text-sm font-semibold text-neutral-700 dark:text-neutral-300">Exchange Controls</h2>
        <Settings2 size={16} className="text-neutral-500 dark:text-neutral-600" />
      </div>

      <div className="p-6 flex-1 flex flex-col gap-8">
        
        {/* Main Start/Stop */}
        <div>
          <button
            onClick={handleToggleExchange}
            disabled={isToggling}
            className={`w-full group relative flex items-center justify-center gap-3 py-4 rounded font-medium transition-all duration-200 ${
              isRunning 
                ? 'bg-rose-100 text-rose-600 border border-rose-200 hover:bg-rose-200/70 dark:bg-rose-900/20 dark:text-rose-500 dark:border-rose-900/50 dark:hover:bg-rose-900/30' 
                : 'bg-emerald-100 text-emerald-600 border border-emerald-200 hover:bg-emerald-200/70 dark:bg-emerald-900/20 dark:text-emerald-500 dark:border-emerald-900/50 dark:hover:bg-emerald-900/30'
            } ${isToggling && 'opacity-70 cursor-not-allowed'}`}
          >
             {getButtonContent()}
             
          </button>
          <div className="mt-3 flex justify-between items-center px-1">
             <span className="text-xs text-neutral-500 font-mono uppercase">Status</span>
             <span className={`text-xs font-mono font-medium ${isRunning ? 'text-emerald-600 dark:text-emerald-500' : 'text-neutral-500'}`}>
                {isToggling ? 'TRANSITIONING...' : (isRunning ? 'ENGINE RUNNING' : 'ENGINE IDLE')}
             </span>
          </div>
        </div>

        <div className="h-px bg-neutral-200 dark:bg-neutral-700 w-full"></div>

        {/* Configuration */}
        <div className="space-y-6">
            
            {/* Market Tick Interval */}
            <div className="flex flex-col gap-2">
                <label className="text-xs text-neutral-500 dark:text-neutral-400 font-medium uppercase tracking-wider">Market Tick Interval</label>
                <div className="grid grid-cols-5 gap-2 mt-2">
                    {[1, 5, 10, 50, 100].map(interval => (
                        <button
                            key={interval}
                            onClick={() => handleIntervalChange(interval)}
                            className={`py-2 text-xs font-mono rounded ${
                                tickInterval === interval
                                    ? 'bg-blue-500 text-white'
                                    : 'bg-neutral-200 dark:bg-neutral-800 hover:bg-neutral-300 dark:hover:bg-neutral-700'
                            }`}
                        >
                            {interval}ms
                        </button>
                    ))}
                </div>
                <p className="text-xs text-neutral-500 dark:text-neutral-600 mt-2">
                    Automated traders flush queued orders once per tick.
                </p>
            </div>
        </div>

      </div>
    </div>
  );

};
export default ExchangeControls;