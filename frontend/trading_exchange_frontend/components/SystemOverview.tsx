import React, { useState, useEffect } from 'react';
import { SystemMetrics } from '../types';

const SystemOverview: React.FC = () => {
  const [metrics, setMetrics] = useState<SystemMetrics>({
    ordersProcessed: 12345,
    avgLatency: 0.824,
    activeOrders: 128,
    connectedClients: 3
  });

  useEffect(() => {
    const interval = setInterval(() => {
      setMetrics(prev => ({
        ordersProcessed: prev.ordersProcessed + Math.floor(Math.random() * 10),
        avgLatency: 0.75 + Math.random() * 0.2,
        activeOrders: 100 + Math.floor(Math.random() * 50),
        connectedClients: 3
      }));
    }, 2000);
    return () => clearInterval(interval);
  }, []);

  const MetricCard = ({ label, value, unit, colorClass }: { label: string, value: string, unit?: string, colorClass?: string }) => (
    <div className="bg-neutral-900 border border-neutral-800 rounded p-4 flex flex-col justify-between h-24">
       <span className="text-[10px] uppercase tracking-wider text-neutral-500 font-medium">{label}</span>
       <div className="flex items-baseline gap-1">
          <span className={`text-2xl font-mono ${colorClass || 'text-neutral-200'}`}>{value}</span>
          {unit && <span className="text-xs text-neutral-600 font-mono">{unit}</span>}
       </div>
    </div>
  );

  return (
    <div className="mt-8">
        <h3 className="text-xs font-semibold text-neutral-500 uppercase tracking-widest mb-3 ml-1">System Overview</h3>
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
            <MetricCard 
                label="Orders Processed" 
                value={metrics.ordersProcessed.toLocaleString()} 
            />
            <MetricCard 
                label="Avg Latency (RTT)" 
                value={metrics.avgLatency.toFixed(3)} 
                unit="ms"
                colorClass={metrics.avgLatency > 1.0 ? 'text-amber-500' : 'text-emerald-500'}
            />
             <MetricCard 
                label="Active Orders" 
                value={metrics.activeOrders.toString()} 
            />
             <MetricCard 
                label="Connected Clients" 
                value={metrics.connectedClients.toString()} 
                unit="/ 10"
            />
        </div>
    </div>
  );
};

export default SystemOverview;