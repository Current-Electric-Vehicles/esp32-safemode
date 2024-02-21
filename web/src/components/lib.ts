
export interface SafemodeAPI {
    ping(): Promise<boolean>;
    restart(): Promise<boolean>;
    otaUpdate(file: File): Promise<boolean>
};

export function delay(time: number): Promise<void> {
    return new Promise<void>((resolve) => {
      setTimeout(resolve, time);
    });
  }
  
  export function fetchWithTimeout(input: RequestInfo | URL, timeout: number, init?: RequestInit): Promise<Response> {
    return new Promise<Response>((resolve, reject) => {
      const controller = new AbortController();
      const id = setTimeout(() => {
        controller.abort();
      }, timeout);
      return fetch(input, {
        ...init,
        signal: controller.signal  
      }).then((r) => {
        resolve(r);
        clearTimeout(id)
      }).catch((e) => reject(e))
    });
  }
  