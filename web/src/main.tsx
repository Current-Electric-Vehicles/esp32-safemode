import './font.css';
import React, { useEffect, useState } from 'react';
import { createRoot } from 'react-dom/client';
import {
  BrowserRouter,
  Route,
  Routes
} from "react-router-dom";
import { API } from './components/api.ts';
import { ConnectedToDeviceContext } from './components/global-contexts.ts';
import './index.css';
import OTAPage from './pages/ota/OTAPage.tsx';

function App() {

  const [connected, setConnected] = useState<boolean>(false);
  
  useEffect(() => {
    const timer = setInterval(() => {
      API.ping()
        .then((r) => {
          if (r && !connected) {
            console.log("Connected to device");
          }
          setConnected(r);
        })
        .catch((e) => {
          console.log("Not connected to device", e);
          setConnected(false);
        })
    }, connected ? 10000 : 5000)
    return () => {
      clearInterval(timer);
    }
  }, [connected])

  return (
    <React.StrictMode>
      <ConnectedToDeviceContext.Provider value={connected}>
        <BrowserRouter>
          <Routes>        
            <Route path="/"  element={<OTAPage />}/>
          </Routes>
        </BrowserRouter>
      </ConnectedToDeviceContext.Provider>
    </React.StrictMode>
  )
}

createRoot(document.getElementById('root')!).render(<App />)
