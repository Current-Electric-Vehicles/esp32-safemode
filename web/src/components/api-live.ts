import { SafemodeAPI, fetchWithTimeout } from "./lib";

const DEFAULT_API_TIMEUT = 60000;

export class SafemodeAPIImpl implements SafemodeAPI {

    public url(url: string): string {
        return `${import.meta.env.VITE_LED_BASE_URL}${url}`
    }

    public ping(): Promise<boolean> {
        return fetchWithTimeout(this.url("/api/ping"), 10000, {method: "POST"})
            .then((r) => r.json())
            .then((r) => r.result);
    }

    public restart(): Promise<boolean> {
        return fetchWithTimeout(this.url("/api/restart"), DEFAULT_API_TIMEUT, {method: "POST"})
            .then((r) => r.json())
            .then((r) => r.result);
    }

    public otaUpdate(file: File): Promise<boolean> {
        return fetch(this.url("/api/update"), {
            method: "POST",
            body: file,
            headers: {
              "X-File-Size": `${file.size}`,
            },
          })
          .then((r) => r.json())
          .then((j) => j.result);
    }

}
