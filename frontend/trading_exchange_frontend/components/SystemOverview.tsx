import React, { useState, useEffect } from 'react';
import { SystemMetrics } from '../types';

const SystemOverview: React.FC = () => {
  const [metrics, setMetrics] = useState<SystemMetrics>({
    ordersProcessed: 0,
    avgLatency: 0.0,
    activeOrders: 0,
    eventQueueDepth: 0,
    throughput: 0.0
  });

  useEffect(() => {
    const fetchMetrics = async () => {
      try {
        const response = await fetch('http://localhost:8000/metrics');
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
        const data: SystemMetrics = await response.json();
        setMetrics(data);
      } catch (error) { console.error("Error fetching system metrics:", error); }
    };
    fetchMetrics();
    const interval = setInterval(fetchMetrics, 2000);
    return () => clearInterval(interval);
  }, []);

  const MetricItem = ({ label, value, unit, colorClass }: { label: string, value: string, unit?: string, colorClass?: string }) => (
    <div className="bg-neutral-950/70 border border-white/5 rounded-lg p-4 flex flex-col justify-center text-center">
       <span className="text-[10px] uppercase tracking-wider text-neutral-500 font-medium mb-1">{label}</span>
       <div className="flex items-baseline justify-center gap-1">
          <span className={`text-xl font-mono ${colorClass || 'text-neutral-200'}`}>{value}</span>
          {unit && <span className="text-xs text-neutral-500 font-mono">{unit}</span>}
       </div>
    </div>
  );

  return (
    <div className="grid grid-cols-2 md:grid-cols-5 gap-4">
        <MetricItem
            label="Orders Processed"
            value={metrics.ordersProcessed.toLocaleString()}
        />
        <MetricItem
            label="Avg Latency"
            value={metrics.avgLatency.toFixed(3)}
            unit="ms"
            colorClass={metrics.avgLatency > 1.0 ? 'text-amber-500' : 'text-emerald-500'}
        />
        <MetricItem
            label="Active Orders"
            value={metrics.activeOrders.toLocaleString()}
        />
        <MetricItem
            label="Event Queue"
            value={metrics.eventQueueDepth.toString()}
            colorClass={metrics.eventQueueDepth > 100 ? 'text-amber-500' : 'text-neutral-200'}
        />
        <MetricItem
            label="Throughput"
            value={metrics.throughput.toFixed(0)}
            unit="ops/s"
        />
    </div>
  );
};

export default SystemOverview;