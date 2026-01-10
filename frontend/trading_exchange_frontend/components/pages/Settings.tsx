import React from 'react';

const Settings: React.FC = () => {
  const handleResetBalance = () => {
    // TODO: Implement API call to reset balance
    console.log("Resetting balance...");
    alert("Trader balance has been reset.");
  }

  const handleResetSystem = () => {
    // TODO: Implement API call to reset system
    console.warn("Resetting system...");
    if (confirm("Are you sure you want to reset the entire system? This action cannot be undone.")) {
      alert("System has been reset.");
    }
  }

  return (
    <div className="p-4 lg:p-8">
      <h1 className="text-2xl font-bold text-neutral-100 mb-6">Settings</h1>
      <div className="max-w-xl space-y-6">
        {/* Account Settings */}
        <div className="bg-neutral-950/70 border border-white/5 rounded-lg p-4 flex items-center justify-between">
          <div>
            <h3 className="text-sm font-medium text-neutral-200">Reset Trader Balance</h3>
            <p className="text-xs text-neutral-500 mt-1">
              Resets your portfolio, positions, and PnL to their default state.
            </p>
          </div>
          <button
            onClick={handleResetBalance}
            className="px-4 py-1.5 text-xs font-semibold bg-amber-600/80 text-white rounded-md hover:bg-amber-600 transition-colors"
          >
            Reset Balance
          </button>
        </div>
        
        {/* System Settings - Danger Zone */}
        <div className="bg-neutral-950/70 border border-rose-500/20 rounded-lg p-4">
            <div className="flex items-center justify-between">
                <div>
                    <h3 className="text-sm font-medium text-rose-400">Danger Zone: Reset System</h3>
                    <p className="text-xs text-neutral-500 mt-1 max-w-md">
                        Warning: This will clear all orders, events, and metrics across the entire exchange. This action cannot be undone.
                    </p>
                </div>
                <button
                    onClick={handleResetSystem}
                    className="px-4 py-1.5 text-xs font-semibold bg-rose-600/80 text-white rounded-md hover:bg-rose-600 transition-colors"
                >
                    Reset System
                </button>
            </div>
        </div>

      </div>
    </div>
  );
};

export default Settings;
