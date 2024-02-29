import { SafemodeAPI, delay } from "./lib";


export class SafemodeAPIDevImpl implements SafemodeAPI {

    constructor() {
        console.log("RUNNING IN DEV MODE, NO BACKEND REQUIRED");
    }

    public url(url: string): string {
        return `${import.meta.env.VITE_LED_BASE_URL}${url}`
    }

    public async ping(): Promise<boolean> {
      await delay(Math.random() * 500);
      return Promise.resolve(true);
    }

    public async restart(): Promise<boolean> {
      await delay(Math.random() * 500);
        return Promise.resolve(true);
    }

    public async otaUpdate(file: File): Promise<boolean> {
      await delay(Math.random() * 500);
      console.log("otaUpdate", file);
      return Promise.resolve(true);
    }

    public async bootIntoApp(): Promise<boolean> {
      await delay(Math.random() * 500);
      console.log("bootIntoApp");
      return Promise.resolve(true);
  }
}
