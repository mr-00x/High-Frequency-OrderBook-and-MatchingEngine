import React, { useState, useEffect, useCallback } from 'react';
import StatsPanel from './components/StatsPanel';
import OrderBook from './components/OrderBook';
import OrderForm from './components/OrderForm';
import TradesFeed from './components/TradesFeed';
import DepthChart from './components/DepthChart';

const API_BASE = import.meta.env.VITE_API_URL || 'http://localhost:8080';

export default function App() {
  const [theme, setTheme] = useState('dark');
  const [book, setBook] = useState({
    symbol: 'STOCK',
    bids: [
      { price: 99.75, quantity: 45 },
      { price: 99.50, quantity: 60 },
      { price: 99.25, quantity: 80 },
      { price: 99.00, quantity: 120 },
      { price: 98.75, quantity: 150 },
      { price: 98.50, quantity: 210 },
      { price: 98.25, quantity: 280 },
      { price: 98.00, quantity: 340 }
    ],
    asks: [
      { price: 100.25, quantity: 50 },
      { price: 100.50, quantity: 75 },
      { price: 100.75, quantity: 95 },
      { price: 101.00, quantity: 130 },
      { price: 101.25, quantity: 160 },
      { price: 101.50, quantity: 220 },
      { price: 101.75, quantity: 290 },
      { price: 102.00, quantity: 360 }
    ]
  });
  const [trades, setTrades] = useState([
    { trade_id: 104, price: 100.25, quantity: 25, buy_order_id: 88, sell_order_id: 82, timestamp: Date.now() * 1000000 },
    { trade_id: 103, price: 99.75, quantity: 40, buy_order_id: 79, sell_order_id: 85, timestamp: (Date.now() - 500) * 1000000 },
    { trade_id: 102, price: 100.00, quantity: 15, buy_order_id: 72, sell_order_id: 75, timestamp: (Date.now() - 1200) * 1000000 }
  ]);
  const [stats, setStats] = useState({
    orders_submitted: 1420,
    orders_filled: 890,
    orders_cancelled: 120,
    trades_executed: 445,
    total_volume: 18450,
    avg_match_latency_ns: 680,
    avg_match_latency_us: 0.68
  });
  const [isConnected, setIsConnected] = useState(false);
  const [selectedPrice, setSelectedPrice] = useState(null);

  // Sync theme with HTML data-theme attribute
  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
  }, [theme]);

  const toggleTheme = () => {
    setTheme(prev => (prev === 'dark' ? 'light' : 'dark'));
  };

  // Poll server state
  const fetchData = useCallback(async () => {
    try {
      const [bookRes, tradesRes, statsRes] = await Promise.all([
        fetch(`${API_BASE}/book?depth=12`),
        fetch(`${API_BASE}/trades?limit=50`),
        fetch(`${API_BASE}/stats`)
      ]);

      if (bookRes.ok && tradesRes.ok && statsRes.ok) {
        const bookData = await bookRes.json();
        const tradesData = await tradesRes.json();
        const statsData = await statsRes.json();

        setBook(bookData);
        setTrades(tradesData.trades || []);
        setStats(statsData);
        setIsConnected(true);
      } else {
        setIsConnected(false);
      }
    } catch {
      setIsConnected(false);
    }
  }, []);

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 350);
    return () => clearInterval(interval);
  }, [fetchData]);

  const handleSubmitOrder = async (orderPayload) => {
    try {
      const res = await fetch(`${API_BASE}/orders`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(orderPayload)
      });
      const data = await res.json();
      fetchData(); // Immediate refresh after submission
      return data;
    } catch (err) {
      return { ok: false, message: err.message };
    }
  };

  return (
    <div className="app-container">
      {/* Navbar Header */}
      <header className="app-header">
        <div className="logo-container">
          <span className="logo-badge">HFT LOB</span>
          <h1 className="app-title">Limit Order Book & Ultra-Fast Matching Engine</h1>
        </div>

        <div className="header-status">
          <div className="status-indicator">
            <span className={`dot ${isConnected ? 'online' : 'offline'}`} />
            <span>{isConnected ? 'Engine Online (8080)' : 'Connecting to Core Engine...'}</span>
          </div>

          {/* Light / Dark Mode Toggle */}
          <button 
            type="button" 
            className="theme-toggle-btn"
            onClick={toggleTheme}
            title="Toggle Light / Dark Mode"
          >
            {theme === 'dark' ? '☀️ Light Mode' : '🌙 Dark Mode'}
          </button>

          <span className="mono" style={{ fontSize: '12px', color: 'var(--accent-cyan)' }}>
            C++17 Core · FIFO L2
          </span>
        </div>
      </header>

      {/* Main Dashboard Layout */}
      <main className="dashboard-grid">
        {/* Top Metric Strip */}
        <StatsPanel stats={stats} isConnected={isConnected} />

        {/* Visual Market Depth Liquidity Curve */}
        <DepthChart book={book} trades={trades} />

        {/* Left: Order Submission Form */}
        <OrderForm 
          onSubmitOrder={handleSubmitOrder} 
          selectedPrice={selectedPrice}
          isConnected={isConnected}
        />

        {/* Center: Live Order Book Ladder */}
        <OrderBook 
          book={book} 
          onSelectPrice={(p) => setSelectedPrice(p)}
        />

        {/* Right: Live Trades Tape */}
        <TradesFeed trades={trades} />
      </main>
    </div>
  );
}
