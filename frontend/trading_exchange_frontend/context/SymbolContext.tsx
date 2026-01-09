import React, { createContext, useState, useContext, useEffect, ReactNode } from 'react';

interface SymbolContextType {
  availableSymbols: string[];
  currentSymbol: string;
  setCurrentSymbol: (symbol: string) => void;
}

const SymbolContext = createContext<SymbolContextType | undefined>(undefined);

export const SymbolProvider: React.FC<{ children: ReactNode }> = ({ children }) => {
  const [availableSymbols, setAvailableSymbols] = useState<string[]>([]);
  const [currentSymbol, setCurrentSymbol] = useState<string>('');

  useEffect(() => {
    const fetchSymbols = async () => {
      try {
        const response = await fetch('http://localhost:8000/symbols');
        if (response.ok) {
          const data = await response.json();
          if (data.symbols && data.symbols.length > 0) {
            setAvailableSymbols(data.symbols);
            setCurrentSymbol(data.symbols[0]);
          }
        }
      } catch (error) {
        console.error('Failed to fetch symbols:', error);
      }
    };
    fetchSymbols();
  }, []);

  const value = { availableSymbols, currentSymbol, setCurrentSymbol };

  return (
    <SymbolContext.Provider value={value}>
      {children}
    </SymbolContext.Provider>
  );
};

export const useSymbol = (): SymbolContextType => {
  const context = useContext(SymbolContext);
  if (context === undefined) {
    throw new Error('useSymbol must be used within a SymbolProvider');
  }
  return context;
};
