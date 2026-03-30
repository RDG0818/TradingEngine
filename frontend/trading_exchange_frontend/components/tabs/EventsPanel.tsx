import React, { useEffect, useState } from 'react';
import { MarketEvent } from '../../types';
import { useApp } from '../../context/AppContext';

const EVENT_COLORS: Record<string, { bg: string; border: string; text: string }> = {
  flash_crash:         { bg: 'bg-red-500/10',    border: 'border-red-500/40',   text: 'text-red-400'    },
  bull_run:            { bg: 'bg-green-500/10',  border: 'border-green-500/40', text: 'text-green-400'  },
  liquidity_squeeze:   { bg: 'bg-yellow-500/10', border: 'border-yellow-500/40',text: 'text-yellow-400' },
  mean_reversion_trap: { bg: 'bg-blue-500/10',   border: 'border-blue-500/40',  text: 'text-blue-400'   },
};

const DEFAULT_COLOR = { bg: 'bg-neutral-800', border: 'border-neutral-600', text: 'text-neutral-300' };

const EventsPanel: React.FC = () => {
  const { apiBase } = useApp();
  const [events, setEvents] = useState<MarketEvent[]>([]);
  const [firing, setFiring] = useState<Set<string>>(new Set());
  const [lastFired, setLastFired] = useState<Record<string, number>>({});

  useEffect(() => {
    fetch(`${apiBase}/events`)
      .then(r => r.json())
      .then(data => setEvents(data.events ?? []))
      .catch(() => {});
  }, [apiBase]);

  const triggerEvent = async (eventId: string) => {
    setFiring(prev => new Set(prev).add(eventId));
    try {
      await fetch(`${apiBase}/events/trigger`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ type: eventId, duration_ticks: 30 }),
      });
      setLastFired(prev => ({ ...prev, [eventId]: Date.now() }));
    } catch {
      // ignore
    } finally {
      // Re-enable after 2s
      setTimeout(() => {
        setFiring(prev => {
          const next = new Set(prev);
          next.delete(eventId);
          return next;
        });
      }, 2000);
    }
  };

  if (events.length === 0) {
    return (
      <div className="h-full flex items-center justify-center text-neutral-600 text-xs">
        Loading events…
      </div>
    );
  }

  return (
    <div className="h-full flex flex-col gap-3 p-3 overflow-y-auto no-scrollbar">
      <p className="text-[10px] text-neutral-600 uppercase tracking-wider">Trigger market event</p>
      <div className="grid grid-cols-2 gap-3">
        {events.map(event => {
          const isFiring = firing.has(event.id);
          const wasFiredRecently = !!lastFired[event.id] && Date.now() - lastFired[event.id] < 3000;
          const colors = EVENT_COLORS[event.id] ?? DEFAULT_COLOR;

          return (
            <button
              key={event.id}
              onClick={() => triggerEvent(event.id)}
              disabled={isFiring}
              className={`relative text-left rounded-md border p-3 transition-all
                ${colors.bg} ${colors.border}
                ${isFiring ? 'opacity-50 cursor-not-allowed' : 'hover:opacity-80 cursor-pointer'}`}
            >
              {wasFiredRecently && (
                <span className="absolute top-1.5 right-2 text-[8px] text-neutral-400 uppercase">fired</span>
              )}
              <div className={`text-[11px] font-medium mb-1 ${colors.text}`}>
                {event.name}
              </div>
              <div className="text-[9px] text-neutral-500 leading-tight">
                {event.description}
              </div>
              <div className="text-[9px] text-neutral-600 mt-1.5">
                {event.default_duration_s}s duration
              </div>
            </button>
          );
        })}
      </div>
    </div>
  );
};

export default EventsPanel;
