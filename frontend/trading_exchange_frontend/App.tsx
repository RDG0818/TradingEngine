import React, { useEffect } from 'react';

const App: React.FC = () => {
  useEffect(() => {
    document.documentElement.classList.add('dark');
  }, []);

  return (
    <div className="flex h-screen w-full bg-neutral-950 text-neutral-200 font-mono">
      <p className="m-auto text-neutral-500">Talat — loading…</p>
    </div>
  );
};

export default App;