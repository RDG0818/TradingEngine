import React from 'react';
import { LayoutDashboard, Activity, Zap, BarChart3, Settings, Database, TrendingUp, Bot } from 'lucide-react';

type Page = 'Dashboard' | 'AutomatedTraders' | 'Settings' | 'ManualTrading';

interface SidebarProps {
  currentPage: Page;
  setCurrentPage: (page: Page) => void;
}

const Sidebar: React.FC<SidebarProps> = ({ currentPage, setCurrentPage }) => {
  const navItems = [
    { key: 'Dashboard', icon: LayoutDashboard, label: 'Dashboard' },
    { key: 'ManualTrading', icon: Zap, label: 'Manual Trading' },
    { key: 'AutomatedTraders', icon: Bot, label: 'Automated Traders' },
    { key: 'Settings', icon: Settings, label: 'Settings' },
  ];

  // These are placeholders for future functionality
  const disabledNavItems = [
    { icon: Activity, label: 'Simulation' },
    { icon: BarChart3, label: 'Analytics' },
    { icon: Database, label: 'Internals' },
  ]

  return (
    <aside className="w-16 lg:w-64 border-r border-neutral-200 dark:border-neutral-800 bg-white dark:bg-neutral-950 flex flex-col justify-between h-full transition-all duration-300">
      <div className="flex flex-col py-6">
        <div className="px-6 mb-8 flex items-center gap-3">
            <div className="w-8 h-8 rounded bg-neutral-100 dark:bg-neutral-800 flex items-center justify-center border border-neutral-200 dark:border-neutral-700">
                <TrendingUp size={16} className="text-emerald-500" />
            </div>
            <span className="hidden lg:block font-bold tracking-tight text-neutral-800 dark:text-neutral-100">Talat</span>
        </div>

        <nav className="flex flex-col gap-1 px-3">
          {navItems.map((item) => (
            <button
              key={item.key}
              onClick={() => setCurrentPage(item.key as Page)}
              className={`flex items-center gap-3 px-3 py-2 rounded-md transition-colors text-sm font-medium ${
                currentPage === item.key
                  ? 'bg-neutral-100 dark:bg-neutral-900 text-emerald-500 dark:text-emerald-400 border border-neutral-200 dark:border-neutral-800'
                  : 'text-neutral-600 dark:text-neutral-500 hover:text-neutral-800 dark:hover:text-neutral-300 hover:bg-neutral-100 dark:hover:bg-neutral-900/50'
              }`}
            >
              <item.icon size={18} />
              <span className="hidden lg:block">{item.label}</span>
            </button>
          ))}
          <div className='h-px w-full bg-neutral-200 dark:bg-neutral-800 my-3'></div>
           {disabledNavItems.map((item) => (
            <button
              key={item.label}
              disabled
              className={`flex items-center gap-3 px-3 py-2 rounded-md transition-colors text-sm font-medium text-neutral-400 dark:text-neutral-700 cursor-not-allowed`}
            >
              <item.icon size={18} />
              <span className="hidden lg:block">{item.label}</span>
            </button>
          ))}
        </nav>
      </div>
      
      <div className="p-4 border-t border-neutral-200 dark:border-neutral-800 hidden lg:block">
        <div className="text-xs text-neutral-500 dark:text-neutral-600 font-mono">
            v2.4.1-stable
        </div>
      </div>
    </aside>
  );
};

export default Sidebar;