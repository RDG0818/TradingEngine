import React from 'react';
import Sidebar from './components/Sidebar';
import Header from './components/Header';
import MarketSnapshot from './components/MarketSnapshot';
import ExchangeControls from './components/ExchangeControls';
import SystemOverview from './components/SystemOverview';
import ActionCenter from './components/ActionCenter';

const App: React.FC = () => {
  return (
    <div className="flex h-screen w-full bg-neutral-950 text-neutral-200 font-sans selection:bg-emerald-900/50 selection:text-emerald-200">
      
      {/* Navigation Sidebar */}
      <Sidebar />

      {/* Main Content Area */}
      <div className="flex-1 flex flex-col min-w-0">
        <Header />

        <main className="flex-1 overflow-y-auto p-4 lg:p-8 no-scrollbar">
          <div className="max-w-6xl mx-auto flex flex-col gap-6">
            
            {/* Top Cards Section */}
            <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 h-auto lg:h-[420px]">
              
              {/* Left Column: Market Data */}
              <div className="lg:col-span-7 h-full">
                <MarketSnapshot />
              </div>

              {/* Right Column: Controls */}
              <div className="lg:col-span-5 h-full">
                <ExchangeControls />
              </div>
            </div>

            {/* CTA Section */}
            <ActionCenter />

            {/* Bottom Metrics */}
            <SystemOverview />
          
          </div>
        </main>
      </div>
    </div>
  );
};

export default App;