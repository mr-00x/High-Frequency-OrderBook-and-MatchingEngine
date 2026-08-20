import React, { useState, useEffect, useCallback } from 'react';
import StatsPanel from './components/StatsPanel';
import OrderBook from './components/OrderBook';
import OrderForm from './components/OrderForm';
import TradesFeed from './components/TradesFeed';

const API_BASE = import.meta.env.VITE_API_URL || 'http://localhost:8080';

export default function App() {
  const [book, setBook] = useState({ bids: [], asks: [], symbol: 'STOCK' });
  const [trades, setTrades] = useState([]);
  const [stats, setStats] = useState(null);
  const [isConnected, setIsConnected] = useState(false);
  const [selectedPrice, setSelectedPrice] = useState(null);

  // Poll server state
  const fetchData = useCallback(async () => {
    try {
      const [bookRes, tradesRes, statsRes] = await Promise.all([
        fetch(`${API_BASE}/book?depth=10`),
        fetch(`${API_BASE}/trades?limit=40`),
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
    const interval = setInterval(fetchData, 400);
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
      fetchData(); // Immediate refresh after order submission
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
          <span className="mono" style={{ fontSize: '12px', color: 'var(--accent-cyan)' }}>
            C++17 Core · FIFO L2
          </span>
        </div>
      </header>

      {/* Main Dashboard Layout */}
      <main className="dashboard-grid">
        {/* Top Metric Strip */}
        <StatsPanel stats={stats} isConnected={isConnected} />

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
