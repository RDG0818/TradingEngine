import React from 'react';
import { LayoutDashboard, Activity, Zap, BarChart3, Settings, Database, TrendingUp } from 'lucide-react';

const Sidebar: React.FC = () => {
  const navItems = [
    { icon: LayoutDashboard, label: 'Dashboard', active: true },
    { icon: Zap, label: 'Manual Trading', active: false },
    { icon: Activity, label: 'Simulation', active: false },
    { icon: BarChart3, label: 'Analytics', active: false },
    { icon: Database, label: 'Internals', active: false },
    { icon: Settings, label: 'Settings', active: false },
  ];

  return (
    <aside className="w-16 lg:w-64 border-r border-neutral-800 bg-neutral-950 flex flex-col justify-between h-full transition-all duration-300">
      <div className="flex flex-col py-6">
        <div className="px-6 mb-8 flex items-center gap-3">
            <div className="w-8 h-8 rounded bg-neutral-800 flex items-center justify-center border border-neutral-700">
                <TrendingUp size={16} className="text-emerald-500" />
            </div>
            <span className="hidden lg:block font-bold tracking-tight text-neutral-100">Talat</span>
        </div>

        <nav className="flex flex-col gap-1 px-3">
          {navItems.map((item) => (
            <button
              key={item.label}
              className={`flex items-center gap-3 px-3 py-2 rounded-md transition-colors text-sm font-medium ${
                item.active
                  ? 'bg-neutral-900 text-emerald-400 border border-neutral-800'
                  : 'text-neutral-500 hover:text-neutral-300 hover:bg-neutral-900/50'
              }`}
            >
              <item.icon size={18} />
              <span className="hidden lg:block">{item.label}</span>
            </button>
          ))}
        </nav>
      </div>
      
      <div className="p-4 border-t border-neutral-800 hidden lg:block">
        <div className="text-xs text-neutral-600 font-mono">
            v2.4.1-stable
        </div>
      </div>
    </aside>
  );
};

export default Sidebar;