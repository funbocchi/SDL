import { window } from "@kit.ArkUI";
import { common } from "@kit.AbilityKit";

export interface WinReqParams {
  
};

export interface WinChParams {
}

export class WindowStageManager {
  private static instance: WindowStageManager | null = null;
  private windowStage: window.WindowStage | null = null;
  private context: common.BaseContext | null = null;
  private isMainWindowLoaded: boolean = false;
  private nameSet: Set<string> = new Set<string>();

  private constructor() {
  }

  static getInstance(): WindowStageManager {
    if (WindowStageManager.instance === null) {
      WindowStageManager.instance = new WindowStageManager();
    }
    return WindowStageManager.instance;
  }
}