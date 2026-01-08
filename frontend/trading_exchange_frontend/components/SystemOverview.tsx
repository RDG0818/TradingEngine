import React, { useState, useEffect } from 'react';
import { SystemMetrics } from '../types';

const SystemOverview: React.FC = () => {
  const [metrics, setMetrics] = useState<SystemMetrics>({
    ordersProcessed: 0,
    avgLatency: 0.0,
    activeOrders: 0
  });

  useEffect(() => {
    const fetchMetrics = async () => {
      try {
        const response = await fetch('http://localhost:8000/metrics'); // Your FastAPI backend address
        if (!response.ok) {
          throw new Error(`HTTP error! status: ${response.status}`);
        }
        const data: SystemMetrics = await response.json();
        setMetrics(data);
      } catch (error) {
        console.error("Error fetching system metrics:", error);
      }
    };

    // Fetch immediately and then every 2 seconds
    fetchMetrics();
    const interval = setInterval(fetchMetrics, 2000);

    return () => clearInterval(interval); // Cleanup on component unmount
  }, []);

  const MetricItem = ({ label, value, unit, colorClass }: { label: string, value: string, unit?: string, colorClass?: string }) => (
    <div className="flex-1 bg-neutral-50 dark:bg-neutral-900 border border-neutral-200 dark:border-neutral-700 rounded-lg p-3 flex items-center justify-between">
       <span className="text-[10px] uppercase tracking-wider text-neutral-500 font-medium">{label}</span>
       <div className="flex items-baseline gap-1">
          <span className={`text-base font-mono ${colorClass || 'text-neutral-800 dark:text-neutral-200'}`}>{value}</span>
          {unit && <span className="text-xs text-neutral-500 dark:text-neutral-600 font-mono">{unit}</span>}
       </div>
    </div>
  );

  return (
    <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        <MetricItem
            label="Orders Processed"
            value={metrics.ordersProcessed.toLocaleString()}
        />
        <MetricItem
            label="Avg Latency (RTT)"
            value={metrics.avgLatency.toFixed(3)}
            unit="ms"
            colorClass={metrics.avgLatency > 1.0 ? 'text-amber-600 dark:text-amber-500' : 'text-emerald-600 dark:text-emerald-500'}
        />
        <MetricItem
            label="Active Orders"
            value={metrics.activeOrders.toString()}
        />
    </div>
  );
};

export default SystemOverview;