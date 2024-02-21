
import { SafemodeAPI } from "./lib";
import { SafemodeAPIDevImpl } from "./api-dev";
import { SafemodeAPIImpl } from "./api-live";

export const API: SafemodeAPI = (import.meta.env.VITE_DEV_MODE == "true")
  ? new SafemodeAPIDevImpl()
  : new SafemodeAPIImpl();
