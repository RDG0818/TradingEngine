import React, { useState } from 'react';
import { ChevronDown, Trash2, Edit, Plus } from 'lucide-react';

// --- Reusable Components ---
const Widget: React.FC<{ title: string; children: React.ReactNode }> = ({ title, children }) => (
    <div className="bg-neutral-50 dark:bg-neutral-900 border border-neutral-200 dark:border-neutral-700 rounded-lg flex flex-col">
        <h2 className="text-sm font-semibold text-neutral-700 dark:text-neutral-300 p-4 border-b border-neutral-200 dark:border-neutral-700 text-center">
            {title}
        </h2>
        <div className="p-6">{children}</div>
    </div>
);

const Input: React.FC<{ label: string; placeholder?: string, type?: string, value?: string, onChange?: (e: React.ChangeEvent<HTMLInputElement>) => void }> = (props) => (
    <div>
        <label className="block text-xs text-neutral-500 dark:text-neutral-400 font-medium mb-1">{props.label}</label>
        <input {...props} className="w-full p-2 rounded-md bg-neutral-100 dark:bg-neutral-800 border border-neutral-200 dark:border-neutral-700 focus:ring-2 focus:ring-blue-500 outline-none"/>
    </div>
);

const Select: React.FC<{ label: string; children: React.ReactNode; value?: string; onChange?: (e: React.ChangeEvent<HTMLSelectElement>) => void }> = (props) => (
    <div>
        <label className="block text-xs text-neutral-500 dark:text-neutral-400 font-medium mb-1">{props.label}</label>
        <div className="relative">
            <select {...props} className="w-full appearance-none p-2 rounded-md bg-neutral-100 dark:bg-neutral-800 border border-neutral-200 dark:border-neutral-700 focus:ring-2 focus:ring-blue-500 outline-none">
                {props.children}
            </select>
            <ChevronDown size={16} className="absolute right-3 top-1/2 -translate-y-1/2 text-neutral-500 pointer-events-none" />
        </div>
    </div>
);

const Button: React.FC<{ children: React.ReactNode, icon?: React.ReactNode, onClick?: () => void, fullWidth?: boolean, variant?: 'primary' | 'danger' }> = ({ children, icon, onClick, fullWidth, variant = 'primary' }) => {
    const baseClasses = "flex items-center justify-center gap-2 px-4 py-2 rounded-md font-sans font-semibold transition-all duration-200 text-sm";
    const widthClass = fullWidth ? 'w-full' : '';
    const colorClass = variant === 'primary' 
        ? 'bg-blue-600 hover:bg-blue-700 text-white'
        : 'bg-rose-600 hover:bg-rose-700 text-white';
    
    return (
        <button onClick={onClick} className={`${baseClasses} ${widthClass} ${colorClass}`}>
            {icon}{children}
        </button>
    );
};

// --- Page Specific Widgets ---

const ActiveTradersWidget = () => {
    const traders = [
        { name: 'MarketMaker-01', type: 'Market Maker', status: 'Running' },
        { name: 'RandomLimit-A', type: 'Random Limit', status: 'Running' },
        { name: 'RandomMarket-B', type: 'Random Market', status: 'Paused' },
    ];

    return (
        <Widget title="Active Automated Traders">
            <ul className="space-y-3">
                {traders.map(trader => (
                    <li key={trader.name} className="flex items-center justify-between p-3 bg-neutral-100 dark:bg-neutral-800/50 rounded-lg">
                        <div className="flex items-center gap-3">
                            <span className={`w-2 h-2 rounded-full ${trader.status === 'Running' ? 'bg-emerald-500' : 'bg-amber-500'}`}></span>
                            <div>
                                <p className="font-semibold text-sm text-neutral-800 dark:text-neutral-200">{trader.name}</p>
                                <p className="text-xs text-neutral-500 dark:text-neutral-400">{trader.type}</p>
                            </div>
                        </div>
                        <div className="flex items-center gap-2">
                            <button className="p-1.5 text-neutral-500 hover:text-blue-500"><Edit size={14} /></button>
                            <button className="p-1.5 text-neutral-500 hover:text-rose-500"><Trash2 size={14} /></button>
                        </div>
                    </li>
                ))}
            </ul>
        </Widget>
    );
}

const AddTraderWidget = () => {
    const [traderType, setTraderType] = useState('market_maker_trader');

    const renderParams = () => {
        switch(traderType) {
            case 'random_market_trader':
                return <Input label="Order Interval (ms)" placeholder="1000" />;
            case 'random_limit_trader':
                return (
                    <>
                        <Input label="Order Interval (ms)" placeholder="1200" />
                        <Input label="Price Range (%)" placeholder="0.5" />
                    </>
                );
            case 'market_maker_trader':
                return (
                    <>
                        <Input label="Spread (%)" placeholder="0.1" />
                        <Input label="Order Size" placeholder="1.0" />
                        <Input label="Update Interval (ms)" placeholder="500" />
                    </>
                );
            default:
                return null;
        }
    }

    return (
        <Widget title="Add New Trader">
            <div className="space-y-4">
                <Input label="Trader Name" placeholder="e.g., MyMarketMaker" />
                <Select label="Trader Type" value={traderType} onChange={e => setTraderType(e.target.value)}>
                    <option value="market_maker_trader">Market Maker Trader</option>
                    <option value="random_limit_trader">Random Limit Trader</option>
                    <option value="random_market_trader">Random Market Trader</option>
                </Select>
                <div className="h-px bg-neutral-200 dark:bg-neutral-700"></div>
                <h3 className="text-xs font-semibold uppercase text-neutral-500 dark:text-neutral-400">Parameters</h3>
                {renderParams()}
                <div className="pt-2">
                    <Button fullWidth icon={<Plus size={16}/>}>Add Trader</Button>
                </div>
            </div>
        </Widget>
    );
}

const EditTraderWidget = () => (
    <Widget title="Edit Trader: MarketMaker-01">
        <div className="space-y-4">
            <Input label="Trader Name" value="MarketMaker-01" />
            <Select label="Trader Type" value="market_maker_trader">
                 <option value="market_maker_trader">Market Maker Trader</option>
            </Select>
            <div className="h-px bg-neutral-200 dark:bg-neutral-700"></div>
            <h3 className="text-xs font-semibold uppercase text-neutral-500 dark:text-neutral-400">Parameters</h3>
            <Input label="Spread (%)" value="0.1" />
            <Input label="Order Size" value="1.0" />
            <Input label="Update Interval (ms)" value="500" />
             <div className="pt-2">
                <Button fullWidth icon={<Edit size={16}/>}>Save Changes</Button>
            </div>
        </div>
    </Widget>
);


const AutomatedTraderSettings: React.FC = () => {
  return (
    <div className="p-4 lg:p-8 flex flex-col h-full">
      <h1 className="text-2xl font-bold text-neutral-800 dark:text-white mb-6">Automated Trader Settings</h1>
      
      <div className="flex-1 overflow-y-auto no-scrollbar">
          <div className="grid grid-cols-1 lg:grid-cols-3 gap-8">
            <div className="lg:col-span-2 space-y-8">
                <ActiveTradersWidget />
                {/* This is a static placeholder to show the edit state */}
                <EditTraderWidget />
            </div>
            <div className="lg:col-span-1">
                <AddTraderWidget />
            </div>
          </div>
      </div>
    </div>
  );
};

export default AutomatedTraderSettings;