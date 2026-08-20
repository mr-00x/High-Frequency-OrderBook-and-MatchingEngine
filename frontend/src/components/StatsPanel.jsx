import React from 'react';

export default function StatsPanel({ stats, isConnected }) {
  const latencyNs = stats ? Math.round((stats.avg_match_latency_us || 0) * 1000) : 0;
  const isSubMicrosecond = stats && (stats.avg_match_latency_us || 0) < 1.0;

  return (
    <div className="stats-banner">
      <div className="glass-panel metric-box">
        <span className="metric-label">Average Match Latency</span>
        <div className="metric-value mono" style={{ color: isSubMicrosecond ? 'var(--buy-color)' : 'var(--accent-cyan)' }}>
          {stats ? `${(stats.avg_match_latency_us || 0).toFixed(2)} µs` : '--'}
        </div>
        <span className="metric-sub mono">
          {stats ? `${latencyNs} ns (Sub-microsecond)` : 'Matching Pipeline'}
        </span>
      </div>

      <div className="glass-panel metric-box">
        <span className="metric-label">Orders Processed</span>
        <div className="metric-value mono">
          {stats ? (stats.orders_submitted || 0).toLocaleString() : '--'}
        </div>
        <span className="metric-sub" style={{ color: 'var(--text-muted)' }}>
          {stats ? `${(stats.orders_filled || 0).toLocaleString()} Filled · ${(stats.orders_cancelled || 0).toLocaleString()} Cancelled` : ''}
        </span>
      </div>

      <div className="glass-panel metric-box">
        <span className="metric-label">Total Trades Executed</span>
        <div className="metric-value mono" style={{ color: 'var(--accent-purple)' }}>
          {stats ? (stats.trades_executed || 0).toLocaleString() : '--'}
        </div>
        <span className="metric-sub" style={{ color: 'var(--text-muted)' }}>
          Matches Completed
        </span>
      </div>

      <div className="glass-panel metric-box">
        <span className="metric-label">Cumulative Volume</span>
        <div className="metric-value mono" style={{ color: 'var(--accent-amber)' }}>
          {stats ? (stats.total_volume || 0).toLocaleString() : '--'}
        </div>
        <span className="metric-sub" style={{ color: 'var(--text-muted)' }}>
          Contracts / Shares
        </span>
      </div>
    </div>
  );
}
