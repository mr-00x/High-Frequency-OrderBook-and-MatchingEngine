import React from 'react';

export default function OrderBook({ book, onSelectPrice }) {
  const bids = book?.bids || [];
  const asks = book?.asks || [];

  // Calculate maximum quantity for depth percentage bars
  const maxQty = Math.max(
    ...bids.map(b => b.quantity),
    ...asks.map(a => a.quantity),
    1
  );

  const bestBid = bids.length > 0 ? bids[0].price : null;
  const bestAsk = asks.length > 0 ? asks[0].price : null;
  const spread = (bestBid && bestAsk) ? (bestAsk - bestBid).toFixed(2) : null;
  const spreadPct = (bestBid && bestAsk) ? (((bestAsk - bestBid) / bestAsk) * 100).toFixed(3) : null;

  // Asks are displayed top-to-bottom descending so lowest ask is closest to spread
  const sortedAsks = [...asks].slice(0, 10).reverse();
  const sortedBids = [...bids].slice(0, 10);

  return (
    <div className="glass-panel orderbook-panel">
      <div className="panel-header">
        <div className="panel-title">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"/>
          </svg>
          Order Book ({book?.symbol || 'STOCK'})
        </div>
        <div className="mono" style={{ fontSize: '11px', color: 'var(--text-muted)' }}>
          Depth L2 (Top 10)
        </div>
      </div>

      <div className="ladder-table-header mono">
        <span>PRICE ($)</span>
        <span style={{ textAlign: 'right' }}>SIZE</span>
        <span style={{ textAlign: 'right' }}>TOTAL</span>
      </div>

      <div className="ladder-rows">
        {/* Asks (Red) */}
        <div style={{ display: 'flex', flexDirection: 'column', justifyContent: 'flex-end', minHeight: '180px' }}>
          {sortedAsks.length === 0 ? (
            <div style={{ padding: '24px', textAlign: 'center', color: 'var(--text-muted)', fontSize: '12px' }}>
              No resting asks
            </div>
          ) : (
            sortedAsks.map((ask, idx) => {
              const depthPct = Math.min(100, Math.round((ask.quantity / maxQty) * 100));
              return (
                <div 
                  key={`ask-${ask.price}-${idx}`} 
                  className="ladder-row mono"
                  onClick={() => onSelectPrice && onSelectPrice(ask.price)}
                >
                  <div className="depth-bar ask" style={{ width: `${depthPct}%` }} />
                  <span style={{ color: 'var(--sell-color)', fontWeight: 600 }}>
                    {ask.price.toFixed(2)}
                  </span>
                  <span style={{ textAlign: 'right', color: 'var(--text-primary)' }}>
                    {ask.quantity.toLocaleString()}
                  </span>
                  <span style={{ textAlign: 'right', color: 'var(--text-muted)' }}>
                    {(ask.price * ask.quantity).toLocaleString(undefined, { maximumFractionDigits: 0 })}
                  </span>
                </div>
              );
            })
          )}
        </div>

        {/* Spread Banner */}
        <div className="spread-divider mono">
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <span style={{ color: 'var(--text-muted)', fontSize: '11px' }}>SPREAD</span>
            <span className="spread-val">${spread ? spread : '--'}</span>
          </div>
          <span style={{ color: 'var(--text-muted)', fontSize: '11px' }}>
            {spreadPct ? `${spreadPct}%` : '0.00%'}
          </span>
        </div>

        {/* Bids (Green) */}
        <div style={{ display: 'flex', flexDirection: 'column', minHeight: '180px' }}>
          {sortedBids.length === 0 ? (
            <div style={{ padding: '24px', textAlign: 'center', color: 'var(--text-muted)', fontSize: '12px' }}>
              No resting bids
            </div>
          ) : (
            sortedBids.map((bid, idx) => {
              const depthPct = Math.min(100, Math.round((bid.quantity / maxQty) * 100));
              return (
                <div 
                  key={`bid-${bid.price}-${idx}`} 
                  className="ladder-row mono"
                  onClick={() => onSelectPrice && onSelectPrice(bid.price)}
                >
                  <div className="depth-bar bid" style={{ width: `${depthPct}%` }} />
                  <span style={{ color: 'var(--buy-color)', fontWeight: 600 }}>
                    {bid.price.toFixed(2)}
                  </span>
                  <span style={{ textAlign: 'right', color: 'var(--text-primary)' }}>
                    {bid.quantity.toLocaleString()}
                  </span>
                  <span style={{ textAlign: 'right', color: 'var(--text-muted)' }}>
                    {(bid.price * bid.quantity).toLocaleString(undefined, { maximumFractionDigits: 0 })}
                  </span>
                </div>
              );
            })
          )}
        </div>
      </div>
    </div>
  );
}
