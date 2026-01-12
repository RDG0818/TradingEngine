import React, { useState, useCallback, useMemo, useEffect } from 'react';
import { Play, Pause, Settings, Plus, X, Bot, ChevronDown, Trash2 } from 'lucide-react';

// --- TYPE DEFINITIONS ---
type TraderStatus = 'running' | 'idle' | 'halted';
type TraderType = 'Market Maker' | 'Random Limit' | 'Random Market';

interface Trader {
  id: string;
  name: string;
  type: TraderType;
  status: TraderStatus;
  pnl: number;
  latency: number;
  ops: number; // Orders per second
  recentHistory: number[]; // For sparkline
  parameters: Record<string, any>;
}

// --- CONSTANTS ---
const TRADER_TYPES: TraderType[] = ['Market Maker', 'Random Limit', 'Random Market'];
const DEFAULT_PARAMS: Record<TraderType, Record<string, string>> = {
    'Market Maker': { mu: '0.0', sigma: '0.1', spread: '0.05', max_quantity: '10' },
    'Random Limit': { lambda: '5.0', 'normal_distribution_variance': '0.2', max_quantity: '10' },
    'Random Market': { lambda: '5.0', max_quantity: '10' },
};

// --- REUSABLE & STYLED COMPONENTS ---

const Pnl: React.FC<{ value: number }> = ({ value }) => {
  const isPositive = value >= 0;
  const color = isPositive ? 'text-emerald-500' : 'text-rose-500';
  const sign = isPositive ? '+' : '';
  return <span className={`${color} font-mono`}>{sign}{value.toFixed(2)}</span>;
};

const TypeBadge: React.FC<{ type: string }> = ({ type }) => {
    const typeMap: Record<string, { label: string; className: string }> = {
      'Market Maker': { label: 'MM', className: 'bg-blue-500/10 text-blue-400 border-blue-500/20' },
      'Random Limit': { label: 'RNDL', className: 'bg-purple-500/10 text-purple-400 border-purple-500/20' },
      'Random Market': { label: 'RNDM', className: 'bg-amber-500/10 text-amber-400 border-amber-500/20' },
    };
    const { label, className } = typeMap[type] || { label: 'CUST', className: 'bg-neutral-500/10 text-neutral-400 border-neutral-500/20' };
    return <span className={`px-1.5 py-0.5 text-[10px] font-semibold tracking-wider rounded-sm border ${className}`}>{label}</span>;
};

const Sparkline: React.FC<{ data: number[]; status: TraderStatus }> = ({ data, status }) => {
    const points = useMemo(() => {
        if (!data || data.length === 0) return "0,15 100,15";
        const width = 100, height = 30, max = Math.max(...data), min = Math.min(...data), range = max - min;
        return data.map((d, i) => `${(i / (data.length - 1)) * width},${height - ((d - min) / (range || 1)) * height}`).join(' ');
    }, [data]);
    return <svg viewBox="0 0 100 30" className="w-full h-12" preserveAspectRatio="none"><polyline fill="none" className={status === 'running' ? 'stroke-emerald-500' : 'stroke-neutral-600'} strokeWidth="1.5" points={points} /></svg>;
};

// --- MAIN PAGE COMPONENTS ---

const TraderCard: React.FC<{ trader: Trader; onEdit: (trader: Trader) => void; onToggleStatus: (id: string) => void; }> = 
({ trader, onEdit, onToggleStatus }) => {
    const isRunning = trader.status === 'running';
    return (
        <div className="bg-neutral-950/70 border border-white/5 rounded-lg p-4 flex flex-col gap-4 transition-all hover:bg-white/[.02] hover:border-white/10 group">
            <div className="flex justify-between items-center">
                <h3 className="font-semibold text-neutral-200">{trader.name}</h3>
                <TypeBadge type={trader.type} />
            </div>
            <div className="flex flex-col gap-1">
                <div className="flex items-center gap-2">
                    <div className="relative flex h-2 w-2">
                        {isRunning && <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-emerald-400 opacity-75"></span>}
                        <span className={`relative inline-flex rounded-full h-2 w-2 ${isRunning ? 'bg-emerald-500' : 'bg-neutral-600'}`}></span>
                    </div>
                    <span className={`text-xs font-medium tracking-wide uppercase ${isRunning ? 'text-emerald-500' : 'text-neutral-500'}`}>{trader.status}</span>
                </div>
                <Sparkline data={trader.recentHistory} status={trader.status} />
            </div>
            <div className="grid grid-cols-4 gap-2 text-center border-t border-b border-white/5 py-3">
                {['OPS', 'Lat (µs)', 'Max Qty', 'PnL'].map(label => <div key={label}><div className="text-xs text-neutral-500">{label}</div>{
                    label === 'PnL' ? <Pnl value={trader.pnl} /> :
                    <div className="font-mono text-neutral-200">{
                        label === 'OPS' ? trader.ops.toFixed(1) :
                        label === 'Lat (µs)' ? (trader.latency * 1000).toFixed(2) :
                        trader.parameters.max_quantity ?? '10'
                    }</div>
                }</div>)}
            </div>
            <div className="flex justify-end gap-2">
                <button onClick={() => onToggleStatus(trader.id)} className="p-2 text-neutral-400 rounded-md hover:bg-white/10 hover:text-white transition-colors">{isRunning ? <Pause size={16} /> : <Play size={16} />}</button>
                <button onClick={() => onEdit(trader)} className="p-2 text-neutral-400 rounded-md hover:bg-white/10 hover:text-white transition-colors"><Settings size={16} /></button>
            </div>
        </div>
    );
};

const FormInput: React.FC<{ label: string; value: string; onChange: (e: React.ChangeEvent<HTMLInputElement>) => void; placeholder?: string }> = 
({ label, value, onChange, placeholder }) => (
    <div>
        <label className="block text-sm text-neutral-400 font-medium mb-1 capitalize">{label.replace(/_/g, ' ')}</label>
        <input type="text" value={value} onChange={onChange} placeholder={placeholder} className="w-full p-2 rounded-md bg-neutral-800 border border-neutral-700 focus:ring-2 focus:ring-blue-500 outline-none" />
    </div>
);

const DeployDrawer: React.FC<{ isOpen: boolean; onClose: () => void; traderToEdit: Trader | null; onSave: (trader: Omit<Trader, 'id' | 'recentHistory'> & { id?: string }) => void; onDelete: (id: string) => void; }> = 
({ isOpen, onClose, traderToEdit, onSave, onDelete }) => {
    const [name, setName] = useState('');
    const [type, setType] = useState<TraderType>('Market Maker');
    const [parameters, setParameters] = useState<Record<string, string>>(DEFAULT_PARAMS['Market Maker']);

    useEffect(() => {
        if (traderToEdit) {
            setName(traderToEdit.name);
            setType(traderToEdit.type as TraderType);
            const stringParams = Object.fromEntries(Object.entries(traderToEdit.parameters).map(([key, value]) => [key, String(value)]));
            setParameters(stringParams);
        } else {
            setName(''); setType('Market Maker'); setParameters(DEFAULT_PARAMS['Market Maker']);
        }
    }, [traderToEdit, isOpen]);

    useEffect(() => {
        if (!traderToEdit) setParameters(DEFAULT_PARAMS[type]);
    }, [type, traderToEdit]);

    const handleParamChange = (key: string, value: string) => setParameters(prev => ({ ...prev, [key]: value }));
    const handleSubmit = () => {
        const numericParams = Object.fromEntries(Object.entries(parameters).map(([key, value]) => [key, parseFloat(value) || 0]));
        onSave({ id: traderToEdit?.id, name, type, parameters: numericParams, status: traderToEdit?.status || 'idle', pnl: traderToEdit?.pnl || 0, latency: traderToEdit?.latency || 0, ops: traderToEdit?.ops || 0 });
        onClose();
    };
    const handleDelete = () => { if (traderToEdit) onDelete(traderToEdit.id); }

    return (
        <div className={`fixed top-0 right-0 h-full w-full md:w-96 bg-neutral-950/95 backdrop-blur-sm border-l border-white/10 shadow-2xl transition-transform duration-300 ease-in-out z-20 ${isOpen ? 'translate-x-0' : 'translate-x-full'}`}>
            <div className="flex flex-col h-full">
                <div className="flex justify-between items-center p-4 border-b border-white/10">
                    <h2 className="text-lg font-semibold flex items-center gap-2"><Bot size={20} />{traderToEdit ? 'Edit Agent' : 'Deploy New Agent'}</h2>
                    <button onClick={onClose} className="p-1 rounded-full hover:bg-white/10"><X size={20} /></button>
                </div>
                <div className="p-6 space-y-6 overflow-y-auto flex-1">
                    <FormInput label="Agent Name" value={name} onChange={e => setName(e.target.value)} placeholder="e.g., MarketMaker-Gamma" />
                    <div>
                        <label className="block text-sm text-neutral-400 font-medium mb-1">Strategy Type</label>
                        <div className="relative">
                            <select value={type} onChange={(e) => setType(e.target.value as TraderType)} className="w-full appearance-none p-2 rounded-md bg-neutral-800 border border-neutral-700 focus:ring-2 focus:ring-blue-500 outline-none" disabled={!!traderToEdit}>
                                {TRADER_TYPES.map(t => <option key={t} value={t} className="bg-neutral-800 text-white">{t}</option>)}
                            </select>
                            <ChevronDown size={16} className="absolute right-3 top-1/2 -translate-y-1/2 text-neutral-500 pointer-events-none" />
                        </div>
                    </div>
                    <div className="border-t border-white/10 pt-4 space-y-4">
                        <h3 className="text-md font-semibold text-neutral-300">Parameters</h3>
                        {Object.keys(parameters).map(key => <FormInput key={key} label={key} value={parameters[key]} onChange={(e) => handleParamChange(key, e.target.value)} placeholder={`e.g., ${DEFAULT_PARAMS[type]?.[key] || '0.0'}`} />)}
                    </div>
                </div>
                <div className="p-4 border-t border-white/10 space-y-2">
                    <button onClick={handleSubmit} className="w-full flex items-center justify-center gap-2 px-4 py-2 rounded-md font-sans font-semibold transition-all duration-200 text-sm bg-blue-600 hover:bg-blue-700 text-white">{traderToEdit ? 'Save Changes' : <><Plus size={16} /> Deploy Agent</>}</button>
                    {traderToEdit && <button onClick={handleDelete} className="w-full flex items-center justify-center gap-2 px-4 py-2 rounded-md font-sans font-semibold transition-all duration-200 text-sm bg-rose-600/20 text-rose-400 hover:bg-rose-600/30 hover:text-rose-300"><Trash2 size={16} /> Delete Agent</button>}
                </div>
            </div>
        </div>
    );
};

const AutomatedTraderSettings: React.FC = () => {
    const [traders, setTraders] = useState<Trader[]>([]);
    const [isDrawerOpen, setIsDrawerOpen] = useState(false);
    const [traderToEdit, setTraderToEdit] = useState<Trader | null>(null);

    const fetchTraders = useCallback(async () => {
      try {
        const response = await fetch('http://localhost:8000/traders');
        if (response.ok) {
          const data = await response.json();
          setTraders(data.traders);
        } else {
          console.error("Failed to fetch traders:", response.statusText);
        }
      } catch (error) { console.error("Failed to fetch traders:", error); }
    }, []);

    useEffect(() => {
        fetchTraders();
        const interval = setInterval(fetchTraders, 5000);
        return () => clearInterval(interval);
    }, [fetchTraders]);

    const handleDeployClick = () => { setTraderToEdit(null); setIsDrawerOpen(true); };
    const handleEditClick = (trader: Trader) => { setTraderToEdit(trader); setIsDrawerOpen(true); };
    const handleCloseDrawer = () => { setIsDrawerOpen(false); setTraderToEdit(null); };

    const handleSaveTrader = useCallback(async (data: Omit<Trader, 'id' | 'recentHistory'> & { id?: string }) => {
        const isEditing = !!data.id;
        const url = isEditing ? `http://localhost:8000/traders/${data.id}` : 'http://localhost:8000/traders';
        const method = isEditing ? 'PUT' : 'POST';

        const payload = isEditing 
            ? { parameters: data.parameters } 
            : { name: data.name, type: data.type, parameters: data.parameters };

        try {
            const response = await fetch(url, {
                method: method,
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });

            if (!response.ok) {
                const error = await response.json();
                throw new Error(error.detail || 'Failed to save trader');
            }
            
            fetchTraders();
            handleCloseDrawer();
        } catch (error) {
            console.error("Failed to save trader:", error);
            alert(`Error: ${error instanceof Error ? error.message : "Unknown error"}`);
        }
    }, [fetchTraders]);

    const handleToggleStatus = useCallback(async (id: string) => {
        try {
            const response = await fetch(`http://localhost:8000/traders/${id}/toggle`, { method: 'POST' });
            if (!response.ok) {
                const error = await response.json();
                throw new Error(error.detail || 'Failed to toggle trader status');
            }
            // Give the backend a moment to process before refetching
            setTimeout(fetchTraders, 250);
        } catch (error) {
            console.error('Failed to toggle trader status:', error);
            alert(`Error: ${error instanceof Error ? error.message : "Unknown error"}`);
        }
    }, [fetchTraders]);
    
    const handleDeleteTrader = useCallback(async (id: string) => {
        if (window.confirm("Are you sure you want to delete this trading agent? This action cannot be undone.")) {
            try {
                const response = await fetch(`http://localhost:8000/traders/${id}`, { method: 'DELETE' });
                if (!response.ok) {
                    const error = await response.json();
                    throw new Error(error.detail || 'Failed to delete trader');
                }
                fetchTraders();
                handleCloseDrawer();
            } catch (error) {
                console.error("Failed to delete trader:", error);
                alert(`Error: ${error instanceof Error ? error.message : "Unknown error"}`);
            }
        }
    }, [fetchTraders]);

    return (
        <div className="p-4 lg:p-8 flex flex-col h-full bg-neutral-950 text-neutral-200">
            <div className="flex justify-between items-center mb-6">
                <h1 className="text-2xl font-bold text-neutral-100 flex items-center gap-3"><Bot size={28} className="text-blue-500" />Automated Traders</h1>
                <button onClick={handleDeployClick} className="flex items-center justify-center gap-2 px-4 py-2 rounded-md font-sans font-semibold transition-all duration-200 text-sm bg-emerald-600/20 text-emerald-400 border border-emerald-500/30 hover:bg-emerald-500/30 hover:border-emerald-500/50"><Plus size={16} />Deploy Agent</button>
            </div>
            <div className="flex-1 overflow-y-auto no-scrollbar -mr-4 pr-4">
                <div className="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-3 gap-6">
                    {traders.map(trader => <TraderCard key={trader.id} trader={trader} onEdit={handleEditClick} onToggleStatus={handleToggleStatus} />)}
                </div>
            </div>
            <DeployDrawer isOpen={isDrawerOpen} onClose={handleCloseDrawer} traderToEdit={traderToEdit} onSave={handleSaveTrader} onDelete={handleDeleteTrader} />
        </div>
    );
};

export default AutomatedTraderSettings;
