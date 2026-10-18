import { FrameNode } from "@kit.ArkUI";
export interface SDLWindowRequestParams {
  id: number;
  title: string;
  xMode: number,
  yMode: number,
  x: number;
  y: number;
  width: number;
  height: number;
  displayIndex: number,
  fullscreen: boolean;
  fullscreenDestop: boolean;
  borderless: boolean;
  hidden: boolean;
  resizable: boolean;
  allowHighDPI: boolean;
}

export const requestWindowReady: (callback: (req: SDLWindowRequestParams) => void) => void;

export const updateLocale: (language: string, region: string) => void;

export const bindNode: (id: string, node: FrameNode) => void;

export const unbindNode: (id: string) => void;