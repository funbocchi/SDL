import { FrameNode } from "@kit.ArkUI";
export interface OHOSWindowRequest {
  cmd: number;
  id: number;
  title: string;
  x: number;
  y: number;
  w: number;
  h: number;
  minW: number;
  minH: number;
  maxW: number;
  maxH: number;
  displayIndex: number;
  borderless: boolean;
  resizable: boolean;
  allowHighdpi: boolean;
  alwaysOnTop: boolean;
  skipTaskbar: boolean;
  utility: boolean;
  tooltip: boolean;
  popupMenu: boolean;
  fullscreen: boolean;
  fullscreenDesktop: boolean;
  shown: boolean;
  hidden: boolean;
  opacity: number;
  brightness: number;
}

export interface OHOSWindowEvent {
  id: number,
  type: number,
  data1: number,
  data2: number
}

export type OHOSWindowRequestCallback = (request: OHOSWindowRequest) => void;

export const registerWindowRequestCallback: (callback: OHOSWindowRequestCallback) => boolean;

export const attachWindowSurface: (node: FrameNode, windowId: number) => void;

export const detachWindowSurface: (windowId: number) => void;

export const notifyWindowReady: (sdlWindowId: number, ohosWindowId: number) => void;

export const notifyWindowEvent: (event: OHOSWindowEvent) => void;