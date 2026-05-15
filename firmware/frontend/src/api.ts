export interface DeviceInfo {
  chip: string;
  revision: number;
  cores: number;
  idfVersion: string;
  freeHeap: number;
  runningPartition: string;
  appPartition: string;
  firmwareVersion: string;
  factoryResetEnabled: boolean;
}

function apiUrl(path: string): string {
  return path;
}

export async function ping(): Promise<boolean> {
  try {
    const res = await fetch(apiUrl("/api/ping"), {
      method: "POST",
      signal: AbortSignal.timeout(5000),
    });
    const json = await res.json();
    return json.result === true;
  } catch {
    return false;
  }
}

export async function restart(): Promise<boolean> {
  const res = await fetch(apiUrl("/api/restart"), { method: "POST" });
  const json = await res.json();
  return json.result === true;
}

export async function bootIntoApp(): Promise<boolean> {
  const res = await fetch(apiUrl("/api/app"), { method: "POST" });
  const json = await res.json();
  return json.result === true;
}

export async function factoryReset(): Promise<boolean> {
  const res = await fetch(apiUrl("/api/factory-reset"), { method: "POST" });
  const json = await res.json();
  return json.result === true;
}

const kChunkSize = 64 * 1024;

async function postJson(path: string, init?: RequestInit): Promise<boolean> {
  const res = await fetch(apiUrl(path), { method: "POST", ...init });
  if (!res.ok) return false;
  try {
    const json = await res.json();
    return json.result === true;
  } catch {
    return false;
  }
}

// Chunked OTA upload. Each chunk is a separate POST so progress reflects
// bytes acknowledged by the device — same behavior on every browser.
// Safari's XHR.upload.onprogress reports OS-buffer fill, not server acks,
// which is why a single big POST jumps and stalls in Safari.
export async function uploadFirmware(
  file: File,
  onProgress?: (pct: number) => void
): Promise<boolean> {
  if (!(await postJson("/api/update/begin"))) {
    return false;
  }

  try {
    for (let offset = 0; offset < file.size; offset += kChunkSize) {
      const end = Math.min(offset + kChunkSize, file.size);
      const chunk = file.slice(offset, end);
      const res = await fetch(apiUrl("/api/update/chunk"), {
        method: "POST",
        headers: { "X-Chunk-Offset": String(offset) },
        body: chunk,
      });
      if (!res.ok) throw new Error("chunk rejected");
      const json = await res.json();
      if (json.result !== true) throw new Error("chunk failed");
      if (onProgress) onProgress(Math.round((end / file.size) * 100));
    }
  } catch {
    await postJson("/api/update/abort").catch(() => {});
    return false;
  }

  return postJson("/api/update/finish");
}

export async function getInfo(): Promise<DeviceInfo> {
  const res = await fetch(apiUrl("/api/info"));
  return res.json();
}
