import { useCallback, useEffect, useRef, useState } from "preact/hooks";
import {
  ping,
  restart,
  bootIntoApp,
  uploadFirmware,
  factoryReset,
  getInfo,
  type DeviceInfo,
} from "./api";

function StatusDot({ connected }: { connected: boolean }) {
  return (
    <span
      className={`inline-block h-2.5 w-2.5 rounded-full ${
        connected ? "bg-green-400" : "bg-red-400"
      }`}
    />
  );
}

export default function App() {
  const [connected, setConnected] = useState(false);
  const [file, setFile] = useState<File | null>(null);
  const [uploading, setUploading] = useState(false);
  const [progress, setProgress] = useState(0);
  const [message, setMessage] = useState<{
    text: string;
    type: "success" | "error";
  } | null>(null);
  const [deviceInfo, setDeviceInfo] = useState<DeviceInfo | null>(null);
  const [showInfo, setShowInfo] = useState(false);
  const [showRebootOptions, setShowRebootOptions] = useState(false);
  const fileInputRef = useRef<HTMLInputElement>(null);

  // Poll connectivity
  useEffect(() => {
    let active = true;
    const poll = async () => {
      while (active) {
        const ok = await ping();
        if (active) setConnected(ok);
        await new Promise((r) => setTimeout(r, 5000));
      }
    };
    poll();
    return () => {
      active = false;
    };
  }, []);

  // Fetch device info when connected
  useEffect(() => {
    if (connected) {
      getInfo()
        .then(setDeviceInfo)
        .catch(() => setDeviceInfo(null));
    }
  }, [connected]);

  const handleUpload = useCallback(async () => {
    if (!file) return;
    setUploading(true);
    setProgress(0);
    setMessage(null);

    try {
      const ok = await uploadFirmware(file, setProgress);
      if (ok) {
        setMessage({ text: "Update successful! Device is rebooting...", type: "success" });
        setFile(null);
        if (fileInputRef.current) fileInputRef.current.value = "";
      } else {
        setMessage({ text: "Update failed. Please try again.", type: "error" });
      }
    } catch {
      setMessage({ text: "Upload error. Check your connection.", type: "error" });
    } finally {
      setUploading(false);
    }
  }, [file]);

  const handleLeaveSafemode = useCallback(async () => {
    if (!confirm("Are you sure you want to leave safe mode and boot into the app?")) return;
    const ok = await bootIntoApp();
    if (ok) {
      setMessage({ text: "Device is rebooting into app...", type: "success" });
    } else {
      setMessage({ text: "Failed to switch partition.", type: "error" });
    }
  }, []);

  const handleRestart = useCallback(async () => {
    const ok = await restart();
    if (ok) {
      setMessage({ text: "Device is restarting...", type: "success" });
    } else {
      setMessage({ text: "Failed to restart device.", type: "error" });
    }
  }, []);

  const handleFactoryReset = useCallback(async () => {
    if (!confirm("This will erase all device settings. Continue?")) return;
    setMessage(null);
    try {
      const ok = await factoryReset();
      if (ok) {
        setMessage({ text: "Factory reset complete.", type: "success" });
        setShowRebootOptions(true);
      } else {
        setMessage({ text: "Factory reset failed.", type: "error" });
      }
    } catch {
      setMessage({ text: "Factory reset error. Check your connection.", type: "error" });
    }
  }, []);

  return (
    <div className="mx-auto flex min-h-dvh max-w-lg flex-col px-4 py-6">
      {/* Header */}
      <header className="mb-6 flex items-center justify-between">
        <h1 className="text-2xl font-bold tracking-tight text-orange-400">SAFEMODE</h1>
        <div className="flex items-center gap-2 text-sm text-stone-400">
          <StatusDot connected={connected} />
          {connected ? "Connected" : "Disconnected"}
        </div>
      </header>

      {/* Message */}
      {message && (
        <div
          className={`mb-4 rounded-lg px-4 py-3 text-sm ${
            message.type === "success"
              ? "bg-green-900/30 text-green-300"
              : "bg-red-900/30 text-red-300"
          }`}
        >
          {message.text}
        </div>
      )}

      {/* Post-reset reboot options */}
      {showRebootOptions && (
        <div className="mb-4 flex gap-3">
          <button
            onClick={async () => { setShowRebootOptions(false); await bootIntoApp(); setMessage({ text: "Rebooting into app...", type: "success" }); }}
            className="flex-1 rounded-lg bg-orange-500 px-4 py-2.5 text-sm font-semibold text-white transition-colors hover:bg-orange-400"
          >
            Reboot into App
          </button>
          <button
            onClick={async () => { setShowRebootOptions(false); await restart(); setMessage({ text: "Restarting in safemode...", type: "success" }); }}
            className="flex-1 rounded-lg bg-stone-700 px-4 py-2.5 text-sm font-medium text-stone-200 transition-colors hover:bg-stone-600"
          >
            Stay in Safemode
          </button>
        </div>
      )}

      {/* OTA Update */}
      <section className="mb-6 rounded-xl bg-stone-800/50 p-5">
        <h2 className="mb-3 text-lg font-semibold text-stone-200">Firmware Update</h2>
        <p className="mb-4 text-sm text-stone-400">
          Select a firmware image (.bin) to upload. The device will reboot after updating.
        </p>

        <input
          ref={fileInputRef}
          type="file"
          accept=".bin"
          onChange={(e) => setFile((e.target as HTMLInputElement).files?.[0] ?? null)}
          disabled={uploading}
          className="mb-4 block w-full text-sm text-stone-400 file:mr-4 file:rounded-lg file:border-0 file:bg-stone-700 file:px-4 file:py-2 file:text-sm file:font-medium file:text-stone-200 hover:file:bg-stone-600"
        />

        {uploading && (
          <div className="mb-4">
            <div className="mb-1 flex justify-between text-xs text-stone-400">
              <span>Uploading...</span>
              <span>{progress}%</span>
            </div>
            <div className="h-2 overflow-hidden rounded-full bg-stone-700">
              <div
                className="h-full rounded-full bg-orange-400 transition-all duration-300"
                style={{ width: `${progress}%` }}
              />
            </div>
          </div>
        )}

        <button
          onClick={handleUpload}
          disabled={!file || uploading || !connected}
          className="w-full rounded-lg bg-orange-500 px-4 py-2.5 text-sm font-semibold text-white transition-colors hover:bg-orange-400 disabled:cursor-not-allowed disabled:opacity-40"
        >
          {uploading ? "Uploading..." : "Update Firmware"}
        </button>
      </section>

      {/* Actions */}
      <section className="mb-6 rounded-xl bg-stone-800/50 p-5">
        <h2 className="mb-3 text-lg font-semibold text-stone-200">Actions</h2>
        <div className="flex gap-3">
          <button
            onClick={handleLeaveSafemode}
            disabled={uploading || !connected}
            className="flex-1 rounded-lg bg-stone-700 px-4 py-2.5 text-sm font-medium text-stone-200 transition-colors hover:bg-stone-600 disabled:cursor-not-allowed disabled:opacity-40"
          >
            Leave Safemode
          </button>
          <button
            onClick={handleRestart}
            disabled={uploading || !connected}
            className="flex-1 rounded-lg bg-stone-700 px-4 py-2.5 text-sm font-medium text-stone-200 transition-colors hover:bg-stone-600 disabled:cursor-not-allowed disabled:opacity-40"
          >
            Restart
          </button>
        </div>
        {deviceInfo?.factoryResetEnabled && (
          <button
            onClick={handleFactoryReset}
            disabled={uploading || !connected}
            className="mt-3 w-full rounded-lg bg-red-900/50 px-4 py-2.5 text-sm font-medium text-red-300 transition-colors hover:bg-red-900/70 disabled:cursor-not-allowed disabled:opacity-40"
          >
            Factory Reset
          </button>
        )}
      </section>

      {/* Device Info */}
      <section className="rounded-xl bg-stone-800/50">
        <button
          onClick={() => setShowInfo(!showInfo)}
          className="flex w-full items-center justify-between px-5 py-3 text-sm font-medium text-stone-400 transition-colors hover:text-stone-200"
        >
          <span>Device Info</span>
          <span className={`transition-transform ${showInfo ? "rotate-180" : ""}`}>
            &#9660;
          </span>
        </button>
        {showInfo && deviceInfo && (
          <div className="border-t border-stone-700/50 px-5 py-3">
            <dl className="grid grid-cols-2 gap-x-4 gap-y-2 text-sm">
              <dt className="text-stone-500">Chip</dt>
              <dd className="text-stone-300">{deviceInfo.chip}</dd>
              <dt className="text-stone-500">Cores</dt>
              <dd className="text-stone-300">{deviceInfo.cores}</dd>
              <dt className="text-stone-500">IDF Version</dt>
              <dd className="text-stone-300">{deviceInfo.idfVersion}</dd>
              <dt className="text-stone-500">Free Heap</dt>
              <dd className="text-stone-300">{(deviceInfo.freeHeap / 1024).toFixed(0)} KB</dd>
              <dt className="text-stone-500">Running</dt>
              <dd className="text-stone-300">{deviceInfo.runningPartition}</dd>
              <dt className="text-stone-500">App Partition</dt>
              <dd className="text-stone-300">{deviceInfo.appPartition}</dd>
              <dt className="text-stone-500">FW Version</dt>
              <dd className="text-stone-300">{deviceInfo.firmwareVersion}</dd>
            </dl>
          </div>
        )}
        {showInfo && !deviceInfo && (
          <div className="border-t border-stone-700/50 px-5 py-3 text-sm text-stone-500">
            Connect to device to view info
          </div>
        )}
      </section>
    </div>
  );
}
