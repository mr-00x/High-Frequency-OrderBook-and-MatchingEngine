import React, { useMemo } from 'react';

export default function DepthChart({ book, trades }) {
  const bids = book?.bids || [];
  const asks = book?.asks || [];

  // Calculate cumulative depth for bids (highest price to lowest)
  const cumulativeBids = useMemo(() => {
    let cum = 0;
    const sorted = [...bids].sort((a, b) => b.price - a.price);
    return sorted.map((b) => {
      cum += b.quantity;
      return { price: b.price, quantity: b.quantity, cumulative: cum };
    });
  }, [bids]);

  // Calculate cumulative depth for asks (lowest price to highest)
  const cumulativeAsks = useMemo(() => {
    let cum = 0;
    const sorted = [...asks].sort((a, b) => a.price - b.price);
    return sorted.map((a) => {
      cum += a.quantity;
      return { price: a.price, quantity: a.quantity, cumulative: cum };
    });
  }, [asks]);

  const maxVolume = useMemo(() => {
    const maxB = cumulativeBids.length > 0 ? cumulativeBids[cumulativeBids.length - 1].cumulative : 0;
    const maxA = cumulativeAsks.length > 0 ? cumulativeAsks[cumulativeAsks.length - 1].cumulative : 0;
    return Math.max(maxB, maxA, 100);
  }, [cumulativeBids, cumulativeAsks]);

  const minPrice = useMemo(() => {
    if (cumulativeBids.length === 0 && cumulativeAsks.length === 0) return 95;
    const lowestBid = cumulativeBids.length > 0 ? cumulativeBids[cumulativeBids.length - 1].price : 95;
    return Math.max(0, lowestBid - 0.5);
  }, [cumulativeBids, cumulativeAsks]);

  const maxPrice = useMemo(() => {
    if (cumulativeBids.length === 0 && cumulativeAsks.length === 0) return 105;
    const highestAsk = cumulativeAsks.length > 0 ? cumulativeAsks[cumulativeAsks.length - 1].price : 105;
    return highestAsk + 0.5;
  }, [cumulativeBids, cumulativeAsks]);

  const priceRange = maxPrice - minPrice || 1;

  const width = 600;
  const height = 180;
  const padding = 25;

  const getX = (price) => {
    return padding + ((price - minPrice) / priceRange) * (width - padding * 2);
  };

  const getY = (cum) => {
    return height - padding - (cum / maxVolume) * (height - padding * 2);
  };

  // Generate SVG path for bids (green area from left up to best bid)
  const bidPath = useMemo(() => {
    if (cumulativeBids.length === 0) return '';
    // reverse to draw from lowest bid price up to best bid
    const reversed = [...cumulativeBids].reverse();
    let path = `M ${getX(reversed[0].price)} ${height - padding}`;
    reversed.forEach((b) => {
      const x = getX(b.price);
      const y = getY(b.cumulative);
      path += ` L ${x} ${y}`;
    });
    // close area at bottom
    path += ` L ${getX(reversed[reversed.length - 1].price)} ${height - padding} Z`;
    return path;
  }, [cumulativeBids, minPrice, priceRange, maxVolume]);

  // Generate SVG path for asks (red area from best ask to highest ask)
  const askPath = useMemo(() => {
    if (cumulativeAsks.length === 0) return '';
    let path = `M ${getX(cumulativeAsks[0].price)} ${height - padding}`;
    cumulativeAsks.forEach((a) => {
      const x = getX(a.price);
      const y = getY(a.cumulative);
      path += ` L ${x} ${y}`;
    });
    path += ` L ${getX(cumulativeAsks[cumulativeAsks.length - 1].price)} ${height - padding} Z`;
    return path;
  }, [cumulativeAsks, minPrice, priceRange, maxVolume]);

  const bestBid = bids.length > 0 ? bids[0].price : null;
  const bestAsk = asks.length > 0 ? asks[0].price : null;
  const midPrice = (bestBid && bestAsk) ? ((bestBid + bestAsk) / 2).toFixed(2) : (bestBid || bestAsk || 100.0).toFixed(2);

  return (
    <div className="glass-panel depth-chart-panel">
      <div className="panel-header">
        <div className="panel-title">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
            <path d="M3 3v18h18M7 16l4-4 4 4 6-6"/>
          </svg>
          Market Depth & Liquidity Curve (L2 Visualizer)
        </div>
        <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
          <span className="mono" style={{ fontSize: '12px', color: 'var(--text-secondary)' }}>
            Mid: <strong style={{ color: 'var(--text-primary)' }}>${midPrice}</strong>
          </span>
          <div style={{ display: 'flex', gap: '8px', fontSize: '11px' }}>
            <span style={{ display: 'flex', alignItems: 'center', gap: '4px', color: 'var(--buy-color)' }}>
              <span style={{ width: '8px', height: '8px', background: 'var(--buy-color)', borderRadius: '2px' }} />
              Bids (${cumulativeBids.length > 0 ? cumulativeBids[cumulativeBids.length - 1].cumulative.toLocaleString() : 0})
            </span>
            <span style={{ display: 'flex', alignItems: 'center', gap: '4px', color: 'var(--sell-color)' }}>
              <span style={{ width: '8px', height: '8px', background: 'var(--sell-color)', borderRadius: '2px' }} />
              Asks (${cumulativeAsks.length > 0 ? cumulativeAsks[cumulativeAsks.length - 1].cumulative.toLocaleString() : 0})
            </span>
          </div>
        </div>
      </div>

      <div className="chart-wrapper" style={{ padding: '8px 12px 4px 12px', position: 'relative' }}>
        {cumulativeBids.length === 0 && cumulativeAsks.length === 0 ? (
          <div style={{ height: `${height}px`, display: 'flex', alignItems: 'center', justifyContent: 'center', color: 'var(--text-muted)', fontSize: '12px' }}>
            Awaiting order book depth data... Start the Simulation Bot or submit orders.
          </div>
        ) : (
          <svg viewBox={`0 0 ${width} ${height}`} style={{ width: '100%', height: '100%', overflow: 'visible' }}>
            <defs>
              <linearGradient id="bidGradient" x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stopColor="var(--buy-color)" stopOpacity="0.45" />
                <stop offset="100%" stopColor="var(--buy-color)" stopOpacity="0.05" />
              </linearGradient>
              <linearGradient id="askGradient" x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stopColor="var(--sell-color)" stopOpacity="0.45" />
                <stop offset="100%" stopColor="var(--sell-color)" stopOpacity="0.05" />
              </linearGradient>
            </defs>

            {/* Grid horizontal lines */}
            {[0.25, 0.5, 0.75].map((ratio) => {
              const y = height - padding - ratio * (height - padding * 2);
              return (
                <g key={ratio}>
                  <line
                    x1={padding}
                    y1={y}
                    x2={width - padding}
                    y2={y}
                    stroke="var(--border-color)"
                    strokeDasharray="4 4"
                    strokeWidth="1"
                  />
                  <text
                    x={width - padding + 4}
                    y={y + 3}
                    fill="var(--text-muted)"
                    fontSize="9"
                    fontFamily="var(--font-mono)"
                  >
                    {Math.round(ratio * maxVolume)}
                  </text>
                </g>
              );
            })}

            {/* Bid Depth Area */}
            {bidPath && (
              <path
                d={bidPath}
                fill="url(#bidGradient)"
                stroke="var(--buy-color)"
                strokeWidth="2"
                strokeLinejoin="round"
              />
            )}

            {/* Ask Depth Area */}
            {askPath && (
              <path
                d={askPath}
                fill="url(#askGradient)"
                stroke="var(--sell-color)"
                strokeWidth="2"
                strokeLinejoin="round"
              />
            )}

            {/* Price labels at bottom */}
            <text x={padding} y={height - 6} fill="var(--text-muted)" fontSize="10" fontFamily="var(--font-mono)">
              ${minPrice.toFixed(2)}
            </text>
            <text x={width / 2} y={height - 6} fill="var(--text-secondary)" fontSize="10" fontFamily="var(--font-mono)" textAnchor="middle">
              ${midPrice} (Mid)
            </text>
            <text x={width - padding} y={height - 6} fill="var(--text-muted)" fontSize="10" fontFamily="var(--font-mono)" textAnchor="end">
              ${maxPrice.toFixed(2)}
            </text>
          </svg>
        )}
      </div>
    </div>
  );
}
