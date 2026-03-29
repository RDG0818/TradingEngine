// frontend/trading_exchange_frontend/components/CandlestickChart.tsx
import React, { useMemo } from 'react';
import {
  ComposedChart,
  XAxis,
  YAxis,
  Tooltip,
  Line,
  Customized,
  ResponsiveContainer,
} from 'recharts';
import { useApp } from '../context/AppContext';
import { Candle } from '../types';

// ─── Custom candle renderer ───────────────────────────────────────────────────

interface CandleLayerProps {
  xAxisMap?: Record<string, any>;
  yAxisMap?: Record<string, any>;
  offset?: { top: number; left: number; width: number; height: number };
  candles: Candle[];
}

function CandleLayer({ xAxisMap, yAxisMap, offset, candles }: CandleLayerProps) {
  if (!xAxisMap || !yAxisMap || !offset) return null;

  const xAxis = Object.values(xAxisMap)[0] as any;
  const yAxis = Object.values(yAxisMap)[0] as any;
  if (!xAxis?.scale || !yAxis?.scale) return null;

  const { left, top } = offset;
  const bandwidth = xAxis.bandwidth ? xAxis.bandwidth() : 8;
  const bodyW = Math.max(2, Math.floor(bandwidth * 0.6));

  return (
    <g transform={`translate(${left}, ${top})`}>
      {candles.map(candle => {
        const cx = xAxis.scale(candle.time) + (bandwidth / 2);
        const openY  = yAxis.scale(candle.open);
        const closeY = yAxis.scale(candle.close);
        const highY  = yAxis.scale(candle.high);
        const lowY   = yAxis.scale(candle.low);
        const isUp   = candle.close >= candle.open;
        const color  = isUp ? '#4ade80' : '#ef4444';
        const bodyTop = Math.min(openY, closeY);
        const bodyH   = Math.max(1, Math.abs(openY - closeY));

        return (
          <g key={candle.time}>
            {/* Wick */}
            <line
              x1={cx} y1={highY}
              x2={cx} y2={lowY}
              stroke={color} strokeWidth={1}
            />
            {/* Body */}
            <rect
              x={cx - bodyW / 2}
              y={bodyTop}
              width={bodyW}
              height={bodyH}
              fill={color}
            />
          </g>
        );
      })}
    </g>
  );
}

// ─── Main component ───────────────────────────────────────────────────────────

const CandlestickChart: React.FC = () => {
  const { candles } = useApp();

  const yDomain = useMemo(() => {
    if (candles.length === 0) return ['auto', 'auto'] as const;
    const lows  = candles.map(c => c.low);
    const highs = candles.map(c => c.high);
    const minP  = Math.min(...lows);
    const maxP  = Math.max(...highs);
    const pad   = (maxP - minP) * 0.05 || maxP * 0.001;
    return [minP - pad, maxP + pad] as [number, number];
  }, [candles]);

  const fmtPrice = (v: number) =>
    `$${v.toLocaleString('en-US', { minimumFractionDigits: 0, maximumFractionDigits: 0 })}`;

  const fmtTime = (v: number) => {
    const d = new Date(v);
    return `${d.getHours().toString().padStart(2,'0')}:${d.getMinutes().toString().padStart(2,'0')}:${d.getSeconds().toString().padStart(2,'0')}`;
  };

  if (candles.length === 0) {
    return (
      <div className="h-full flex items-center justify-center text-neutral-600 text-xs">
        Waiting for trades…
      </div>
    );
  }

  return (
    <ResponsiveContainer width="100%" height="100%">
      <ComposedChart
        data={candles}
        margin={{ top: 8, right: 48, bottom: 4, left: 8 }}
      >
        <XAxis
          dataKey="time"
          tickFormatter={fmtTime}
          tick={{ fill: '#555', fontSize: 9 }}
          axisLine={{ stroke: '#1a1a1a' }}
          tickLine={false}
          interval="preserveStartEnd"
          minTickGap={60}
        />
        <YAxis
          domain={yDomain}
          tickFormatter={fmtPrice}
          tick={{ fill: '#555', fontSize: 9 }}
          axisLine={false}
          tickLine={false}
          width={56}
          orientation="right"
        />
        <Tooltip
          contentStyle={{ background: '#111', border: '1px solid #2a2a2a', borderRadius: 4, fontSize: 10 }}
          labelFormatter={fmtTime}
          formatter={(value: number) => [`$${value.toFixed(2)}`, '']}
          cursor={{ stroke: '#333', strokeWidth: 1 }}
        />
        {/* Invisible line — establishes the y scale domain for Recharts */}
        <Line dataKey="close" dot={false} stroke="transparent" isAnimationActive={false} />
        {/* Custom candle renderer */}
        <Customized
          component={(props: any) => (
            <CandleLayer {...props} candles={candles} />
          )}
        />
      </ComposedChart>
    </ResponsiveContainer>
  );
};

export default CandlestickChart;
