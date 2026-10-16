
export interface SDLWindowRequestParams {
  id: number;
  title: string;
  x: number;
  y: number;
  width: number;
  height: number;
  fullscreen: boolean;
  borderless: boolean;
}

export const requestWindowReady: (callback: (req: SDLWindowRequestParams) => void) => void;

export const updateLocale: (language: string, region: string) => void;