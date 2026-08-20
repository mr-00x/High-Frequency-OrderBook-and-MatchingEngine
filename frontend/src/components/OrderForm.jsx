import React, { useState, useEffect, useRef } from 'react';

export default function OrderForm({ onSubmitOrder, selectedPrice, isConnected }) {
  const [side, setSide] = useState('buy');
  const [orderType, setOrderType] = useState('limit');
  const [price, setPrice] = useState('100.00');
  const [quantity, setQuantity] = useState('10');
  const [toast, setToast] = useState(null);
  const [isBotActive, setIsBotActive] = useState(false);
  const botIntervalRef = useRef(null);

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

  // Automated Market Maker / Flow Simulation Bot
  useEffect(() => {
    if (isBotActive) {
      botIntervalRef.current = setInterval(() => {
        const randSide = Math.random() > 0.5 ? 'buy' : 'sell';
        const randType = Math.random() > 0.15 ? 'limit' : 'market';
        // Cluster prices around 100.00 +- 2.50
        const basePrice = 100.0;
        const offset = (Math.random() * 4.0 - 2.0);
        const simPrice = parseFloat((basePrice + offset).toFixed(2));
        const simQty = Math.floor(Math.random() * 30) + 5;

        onSubmitOrder({
          side: randSide,
          type: randType,
          price: randType === 'limit' ? simPrice : 0,
          quantity: simQty
        });
      }, 350);
    } else {
      if (botIntervalRef.current) clearInterval(botIntervalRef.current);
    }
    return () => {
      if (botIntervalRef.current) clearInterval(botIntervalRef.current);
    };
  }, [isBotActive, onSubmitOrder]);

  const handleStressTest = async () => {
    showToast('Executing 50 rapid simulated orders...', 'success');
    for (let i = 0; i < 50; i++) {
      const randSide = Math.random() > 0.5 ? 'buy' : 'sell';
      const p = parseFloat((100.0 + (Math.random() * 3.0 - 1.5)).toFixed(2));
      const q = Math.floor(Math.random() * 20) + 1;
      onSubmitOrder({
        side: randSide,
        type: 'limit',
        price: p,
        quantity: q
      });
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
        <span className="metric-label" style={{ fontSize: '10px' }}>Simulation & Flow Generator</span>
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
          ⚡ Blast 50 Rapid Limit Orders
        </button>
      </div>
    </div>
  );
}
