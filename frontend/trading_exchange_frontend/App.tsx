import React, { useState, useEffect } from 'react';
import Sidebar from './components/Sidebar';
import Header from './components/Header';
import MarketSnapshot from './components/MarketSnapshot';
import ExchangeControls from './components/ExchangeControls';
import SystemOverview from './components/SystemOverview';
import Settings from './components/pages/Settings';
import AutomatedTraderSettings from './components/pages/AutomatedTraderSettings';
import ManualTrading from './components/pages/ManualTrading';
import LiveEventLog from './components/LiveEventLog';

type Page = 'Dashboard' | 'AutomatedTraders' | 'Settings' | 'ManualTrading';

const App: React.FC = () => {
  const [currentPage, setCurrentPage] = useState<Page>('Dashboard');
  const [theme, setTheme] = useState(localStorage.getItem('theme') || 'dark');

  useEffect(() => {
    const root = window.document.documentElement;
    if (theme === 'dark') {
      root.classList.add('dark');
    } else {
      root.classList.remove('dark');
    }
    localStorage.setItem('theme', theme);
  }, [theme]);

  const renderPage = () => {
    switch (currentPage) {
      case 'ManualTrading':
        return <ManualTrading />;
      case 'AutomatedTraders':
        return <AutomatedTraderSettings />;
      case 'Settings':
        return <Settings theme={theme} setTheme={setTheme} />;
      case 'Dashboard':
      default:
        return (
          <div className="max-w-6xl mx-auto flex flex-col gap-6">
            <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 h-auto lg:min-h-[420px]">
              <div className="lg:col-span-7 h-full">
                <MarketSnapshot />
              </div>
              <div className="lg:col-span-5 h-full">
                <ExchangeControls />
              </div>
            </div>
            <div className="h-[240px]">
                <LiveEventLog />
            </div>
            <SystemOverview />
          </div>
        );
    }
  };

  return (
    <div className="flex h-screen w-full bg-white dark:bg-neutral-950 text-neutral-800 dark:text-neutral-200 font-sans selection:bg-emerald-900/50 selection:text-emerald-200">
      <Sidebar currentPage={currentPage} setCurrentPage={setCurrentPage} />
      <div className="flex-1 flex flex-col min-w-0">
        <Header />
        <main className="flex-1 overflow-y-auto p-4 lg:p-8 no-scrollbar">
          {renderPage()}
        </main>
      </div>
    </div>
  );
};

export default App;