export interface DeviceInfo {
  chip: string;
  revision: number;
  cores: number;
  idfVersion: string;
  freeHeap: number;
  runningPartition: string;
  appPartition: string;
  firmwareVersion: string;
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

export function uploadFirmware(
  file: File,
  onProgress?: (pct: number) => void
): Promise<boolean> {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open("POST", apiUrl("/api/update"));
    xhr.setRequestHeader("X-File-Size", String(file.size));

    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable && onProgress) {
        onProgress(Math.round((e.loaded / e.total) * 100));
      }
    };

    xhr.onload = () => {
      try {
        const json = JSON.parse(xhr.responseText);
        resolve(json.result === true);
      } catch {
        resolve(false);
      }
    };

    xhr.onerror = () => reject(new Error("Upload failed"));
    xhr.ontimeout = () => reject(new Error("Upload timed out"));
    xhr.timeout = 120000;

    xhr.send(file);
  });
}

export async function getInfo(): Promise<DeviceInfo> {
  const res = await fetch(apiUrl("/api/info"));
  return res.json();
}
