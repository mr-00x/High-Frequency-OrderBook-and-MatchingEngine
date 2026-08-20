import React, { useState, useEffect, useRef } from 'react';

export default function OrderForm({ onSubmitOrder, selectedPrice, isConnected }) {
  const [side, setSide] = useState('buy');
  const [orderType, setOrderType] = useState('limit');
  const [price, setPrice] = useState('100.00');
  const [quantity, setQuantity] = useState('10');
  const [toast, setToast] = useState(null);
  const [isBotActive, setIsBotActive] = useState(false);
  const botIntervalRef = useRef(null);

  // Dynamic state for random-walk market simulation
  const midPriceRef = useRef(100.0);
  const volatilityRef = useRef(0.15);

  useEffect(() => {
    if (selectedPrice) {
      setPrice(selectedPrice.toFixed(2));
    }
  }, [selectedPrice]);

  const showToast = (msg, type = 'success') => {
    setToast({ msg, type });
    setTimeout(() => setToast(null), 3000);
  };

  const handleSubmit = async (e) => {
    if (e) e.preventDefault();
    const p = orderType === 'limit' ? parseFloat(price) : 0;
    const q = parseInt(quantity, 10);

    if (isNaN(q) || q <= 0) {
      showToast('Quantity must be greater than 0', 'error');
      return;
    }
    if (orderType === 'limit' && (isNaN(p) || p <= 0)) {
      showToast('Price must be greater than 0.00', 'error');
      return;
    }

    try {
      const res = await onSubmitOrder({
        side,
        type: orderType,
        price: p,
        quantity: q
      });
      if (res && res.ok) {
        showToast(`Order #${res.order_id} Submitted!`, 'success');
      } else {
        showToast(res?.message || 'Failed to submit order', 'error');
      }
    } catch (err) {
      showToast(err.message || 'Network Error', 'error');
    }
  };

  // High-Frequency Market Maker Simulation Bot (Geometric Brownian Motion + Order Flow)
  useEffect(() => {
    if (isBotActive) {
      botIntervalRef.current = setInterval(() => {
        // Micro-Brownian random walk with mean-reversion toward 100.00
        const drift = (100.0 - midPriceRef.current) * 0.05;
        const shock = (Math.random() - 0.5) * volatilityRef.current;
        midPriceRef.current = Math.max(90.0, Math.min(110.0, midPriceRef.current + drift + shock));

        // Realistic Order Size distribution
        const sizeRand = Math.random();
        let simQty;
        if (sizeRand < 0.70) {
          // Retail lots: 5, 10, 15, 20, 25, 50
          simQty = [5, 10, 15, 20, 25, 50][Math.floor(Math.random() * 6)];
        } else if (sizeRand < 0.95) {
          // Institutional blocks: 75, 100, 150, 200, 300
          simQty = [75, 100, 150, 200, 300][Math.floor(Math.random() * 5)];
        } else {
          // Whale blocks: 500, 750, 1000
          simQty = [500, 750, 1000][Math.floor(Math.random() * 3)];
        }

        const flowType = Math.random();

        // 75% Passive Market Making (Placing Bids below mid, Asks above mid)
        if (flowType < 0.75) {
          const isBuy = Math.random() > 0.5;
          const spreadOffset = parseFloat(((Math.random() * 0.80) + 0.05).toFixed(2));
          const simPrice = isBuy
            ? parseFloat((midPriceRef.current - spreadOffset).toFixed(2))
            : parseFloat((midPriceRef.current + spreadOffset).toFixed(2));

          onSubmitOrder({
            side: isBuy ? 'buy' : 'sell',
            type: 'limit',
            price: Math.max(0.01, simPrice),
            quantity: simQty
          });
        } 
        // 25% Aggressive Crossing Orders (Market orders & crossing limits triggering trades)
        else {
          const isAggressiveBuy = Math.random() > 0.5;
          const isMarket = Math.random() > 0.4;
          const crossPrice = isAggressiveBuy
            ? parseFloat((midPriceRef.current + 0.10).toFixed(2))
            : parseFloat((midPriceRef.current - 0.10).toFixed(2));

          onSubmitOrder({
            side: isAggressiveBuy ? 'buy' : 'sell',
            type: isMarket ? 'market' : 'limit',
            price: isMarket ? 0 : crossPrice,
            quantity: Math.min(simQty, 100)
          });
        }
      }, 200); // 5 simulated events per second
    } else {
      if (botIntervalRef.current) clearInterval(botIntervalRef.current);
    }
    return () => {
      if (botIntervalRef.current) clearInterval(botIntervalRef.current);
    };
  }, [isBotActive, onSubmitOrder]);

  // Realistic Market Maker Burst (50 orders across a full order book ladder)
  const handleStressTest = async () => {
    showToast('Injecting 50 realistic multi-level limit & market orders...', 'success');
    const mid = midPriceRef.current;
    
    // Inject layered bids
    for (let i = 1; i <= 20; i++) {
      const p = parseFloat((mid - (i * 0.15) - (Math.random() * 0.05)).toFixed(2));
      const q = Math.floor(Math.random() * 40) + 10;
      onSubmitOrder({ side: 'buy', type: 'limit', price: p, quantity: q });
    }

    // Inject layered asks
    for (let i = 1; i <= 20; i++) {
      const p = parseFloat((mid + (i * 0.15) + (Math.random() * 0.05)).toFixed(2));
      const q = Math.floor(Math.random() * 40) + 10;
      onSubmitOrder({ side: 'sell', type: 'limit', price: p, quantity: q });
    }

    // Inject 10 aggressive crossing executions
    for (let i = 0; i < 10; i++) {
      const isBuy = Math.random() > 0.5;
      const q = Math.floor(Math.random() * 25) + 5;
      onSubmitOrder({ side: isBuy ? 'buy' : 'sell', type: 'market', price: 0, quantity: q });
    }
  };

  return (
    <div className="glass-panel order-form-panel">
      <div className="panel-title">
        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
          <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4M17 8l-5-5-5 5M12 3v12"/>
        </svg>
        Place Order
      </div>

      {toast && (
        <div className={`toast ${toast.type}`}>
          {toast.msg}
        </div>
      )}

      <form onSubmit={handleSubmit} style={{ display: 'flex', flexDirection: 'column', gap: '14px' }}>
        {/* Side Switcher */}
        <div className="side-toggle">
          <button
            type="button"
            className={`side-btn ${side === 'buy' ? 'active buy' : ''}`}
            onClick={() => setSide('buy')}
          >
            BUY (BID)
          </button>
          <button
            type="button"
            className={`side-btn ${side === 'sell' ? 'active sell' : ''}`}
            onClick={() => setSide('sell')}
          >
            SELL (ASK)
          </button>
        </div>

        {/* Order Type */}
        <div className="type-toggle">
          <button
            type="button"
            className={`type-btn ${orderType === 'limit' ? 'active' : ''}`}
            onClick={() => setOrderType('limit')}
          >
            Limit Order
          </button>
          <button
            type="button"
            className={`type-btn ${orderType === 'market' ? 'active' : ''}`}
            onClick={() => setOrderType('market')}
          >
            Market Order
          </button>
        </div>

        {/* Price Input */}
        {orderType === 'limit' && (
          <div className="input-group">
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <label className="input-label">Limit Price</label>
              <span style={{ fontSize: '11px', color: 'var(--text-muted)' }}>USD</span>
            </div>
            <div className="input-wrapper">
              <span className="mono" style={{ color: 'var(--text-muted)', marginRight: '6px' }}>$</span>
              <input
                type="number"
                step="0.01"
                className="input-field mono"
                value={price}
                onChange={(e) => setPrice(e.target.value)}
                placeholder="100.00"
                required
              />
            </div>
          </div>
        )}

        {/* Quantity Input */}
        <div className="input-group">
          <div style={{ display: 'flex', justifyContent: 'space-between' }}>
            <label className="input-label">Quantity</label>
            <span style={{ fontSize: '11px', color: 'var(--text-muted)' }}>Contracts</span>
          </div>
          <div className="input-wrapper">
            <input
              type="number"
              min="1"
              className="input-field mono"
              value={quantity}
              onChange={(e) => setQuantity(e.target.value)}
              placeholder="10"
              required
            />
            <span className="input-suffix">LOTS</span>
          </div>
        </div>

        {/* Quick Quantity Presets */}
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '6px' }}>
          {[10, 50, 100, 500].map((preset) => (
            <button
              key={preset}
              type="button"
              className="sim-btn mono"
              style={{ fontSize: '11px', padding: '4px' }}
              onClick={() => setQuantity(preset.toString())}
            >
              {preset}
            </button>
          ))}
        </div>

        {/* Submit Button */}
        <button
          type="submit"
          className={`submit-btn ${side}`}
          disabled={!isConnected}
          style={{ opacity: isConnected ? 1 : 0.6 }}
        >
          {isConnected 
            ? `${side.toUpperCase()} ${orderType.toUpperCase()}` 
            : 'ENGINE OFFLINE'}
        </button>
      </form>

      {/* Interactive Simulation Controls */}
      <div className="sim-controls">
        <span className="metric-label" style={{ fontSize: '10px' }}>HFT Simulation & Flow Generator</span>
        <button
          type="button"
          className={`sim-btn ${isBotActive ? 'active' : ''}`}
          onClick={() => setIsBotActive(!isBotActive)}
        >
          <span className={`dot ${isBotActive ? 'online' : ''}`} style={{ width: '6px', height: '6px' }} />
          {isBotActive ? 'Pause Order Generator Bot' : 'Start Auto-Order Flow Bot'}
        </button>

        <button
          type="button"
          className="sim-btn"
          onClick={handleStressTest}
        >
          ⚡ Blast 50 Realistic Flow Orders
        </button>
      </div>
    </div>
  );
}
