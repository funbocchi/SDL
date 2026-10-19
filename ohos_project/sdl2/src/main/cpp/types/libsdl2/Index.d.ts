import { FrameNode } from "@kit.ArkUI";
export interface SDLWindowRequestParams {
  id: number;
  title: string;
  x: number;
  px_x_force: boolean;
  y: number;
  px_y_force: boolean;
  width: number;
  height: number;
  borderless: boolean;
  hidden: boolean;
  resizable: boolean;
  allowHighDPI: boolean;
}

export const requestWindowReady: (callback: (req: SDLWindowRequestParams) => void) => void;

export const updateLocale: (language: string, region: string) => void;

export const bindNode: (id: string, node: FrameNode) => void;

export const unbindNode: (id: string) => void;