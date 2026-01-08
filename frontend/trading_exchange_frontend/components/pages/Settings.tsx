import React from 'react';

interface SettingsProps {
  theme: string;
  setTheme: (theme: string) => void;
}

const Settings: React.FC<SettingsProps> = ({ theme, setTheme }) => {
  const toggleTheme = () => {
    setTheme(theme === 'dark' ? 'light' : 'dark');
  };

  return (
    <div className="p-4 lg:p-8">
      <h1 className="text-2xl font-bold text-neutral-800 dark:text-white">Settings</h1>
      <div className="mt-6 max-w-md">
        <div className="flex items-center justify-between p-4 bg-neutral-100 dark:bg-neutral-900 rounded-lg border border-neutral-200 dark:border-neutral-800">
          <div>
            <span className="block text-sm font-medium text-neutral-800 dark:text-neutral-200">Theme</span>
            <span className="block text-xs text-neutral-500 dark:text-neutral-400">
              Switch between light and dark mode.
            </span>
          </div>
          <button
            onClick={toggleTheme}
            className={`relative inline-flex h-6 w-11 items-center rounded-full transition-colors focus:outline-none ${
              theme === 'dark' ? 'bg-blue-600' : 'bg-neutral-300'
            }`}
          >
            <span
              className={`${
                theme === 'dark' ? 'translate-x-6' : 'translate-x-1'
              } inline-block h-4 w-4 transform rounded-full bg-white transition-transform`}
            />
          </button>
        </div>
      </div>
    </div>
  );
};

export default Settings;
