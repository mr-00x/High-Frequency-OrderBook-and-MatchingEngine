import React from 'react';

export default function TradesFeed({ trades }) {
  const tradeList = trades || [];

  return (
    <div className="glass-panel trades-panel">
      <div className="panel-header">
        <div className="panel-title">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z"/>
          </svg>
          Trade Tape (Recent Matches)
        </div>
        <span className="mono" style={{ fontSize: '11px', color: 'var(--text-muted)' }}>
          {tradeList.length} trades
        </span>
      </div>

      <div className="ladder-table-header mono">
        <span>PRICE ($)</span>
        <span style={{ textAlign: 'right' }}>SIZE</span>
        <span style={{ textAlign: 'right' }}>TRADE ID</span>
      </div>

      <div className="trades-list">
        {tradeList.length === 0 ? (
          <div style={{ padding: '36px', textAlign: 'center', color: 'var(--text-muted)', fontSize: '12px' }}>
            No executions yet. Submit crossing orders or start the flow generator!
          </div>
        ) : (
          tradeList.map((t, idx) => {
            const isAggressiveBuy = t.buy_order_id > t.sell_order_id;
            return (
              <div key={`trade-${t.trade_id}-${idx}`} className="trade-item mono">
                <span style={{ 
                  color: isAggressiveBuy ? 'var(--buy-color)' : 'var(--sell-color)',
                  fontWeight: 600
                }}>
                  {t.price.toFixed(2)}
                </span>
                <span style={{ textAlign: 'right', color: 'var(--text-primary)' }}>
                  {t.quantity.toLocaleString()}
                </span>
                <span style={{ textAlign: 'right', color: 'var(--text-muted)', fontSize: '11px' }}>
                  #{t.trade_id}
                </span>
              </div>
            );
          })
        )}
      </div>
    </div>
  );
}
