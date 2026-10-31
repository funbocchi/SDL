import { FrameNode, window } from "@kit.ArkUI";
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

export type OHOSWindowRequestCallback = (request: OHOSWindowRequest) => void;

export const registerWindowRequestCallback: (callback: OHOSWindowRequestCallback) => boolean;

export const attachWindowSurface: (node: FrameNode, windowId: number) => void;

export const detachWindowSurface: (windowId: number) => void;

export const notifyWindowReady: (sdlWindowId: number, ohosWindowId: number) => void;

export const notifyWindowDestroyed: (sdlWindowId: number) => void;

export const notifyWindowEvent: (sdlWindowId: number, event: window.WindowEventType) => void;

export const notifyWindowStatusChange: (sdlWindowId: number, status: window.WindowStatusType) => void;

export const notifyWindowStatusDidChange: (sdlWindowId: number, status: window.WindowStatusType) => void;

export const notifyWindowSizeChange: (sdlWindowId: number, size: window.Size) => void;

export const notifyWindowRectChange: (sdlWindowId: number, options: window.RectChangeOptions) => void;

export const notifyWindowVisibilityChange: (sdlWindowId: number, visible: boolean) => void;

export const notifyWindowAvoidAreaChange: (sdlWindowId: number, options: window.AvoidAreaOptions) => void;

export const notifyWindowSystemAvoidAreaChange: (sdlWindowId: number, area: window.AvoidArea) => void;

export const notifyWindowKeyboardHeightChange: (sdlWindowId: number, height: number) => void;

export const notifyWindowKeyboardWillShow: (sdlWindowId: number, info: window.KeyboardInfo) => void;

export const notifyWindowKeyboardDidShow: (sdlWindowId: number, info: window.KeyboardInfo) => void;

export const notifyWindowKeyboardWillHide: (sdlWindowId: number, info: window.KeyboardInfo) => void;

export const notifyWindowKeyboardDidHide: (sdlWindowId: number, info: window.KeyboardInfo) => void;

export const notifyWindowTouchOutside: (sdlWindowId: number) => void;

export const notifyWindowDisplayIdChange: (sdlWindowId: number, displayId: number) => void;

export const notifyWindowSystemDensityChange: (sdlWindowId: number, density: number) => void;

export const notifyWindowNoInteractionDetected: (sdlWindowId: number) => void;

export const notifyWindowScreenshot: (sdlWindowId: number) => void;

export const notifyWindowScreenshotAppEvent: (sdlWindowId: number, event: window.ScreenshotEventType) => void;

export const notifyWindowDialogTargetTouch: (sdlWindowId: number) => void;

export const notifyWindowHighlightChange: (sdlWindowId: number, highlight: boolean) => void;

export const notifyWindowTitleButtonRectChange: (sdlWindowId: number, rect: window.TitleButtonRect) => void;

export const notifyWindowFreeWindowModeChange: (sdlWindowId: number, free: boolean) => void;

export const notifyWindowOcclusionStateChange: (sdlWindowId: number, state: window.OcclusionState) => void;

export const notifyWindowFrameMetricsMeasured: (sdlWindowId: number, metrics: window.FrameMetrics) => void;

export const notifyWindowRotationChange: (sdlWindowId: number, info: window.RotationChangeInfo) => void;

export const notifyWindowWillClose: (sdlWindowId: number) => void;

export const notifyWindowSubWindowClose: (sdlWindowId: number) => void;