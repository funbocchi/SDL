import { AbilityConstant, Configuration, UIAbility, Want } from "@kit.AbilityKit"
import { window } from '@kit.ArkUI'
import { OHOSWindowRequest, OHOSWindowRequestCallback, registerWindowRequestCallback } from "libsdl2.so";
import { SDL2LocalManager } from "../manager/SDL2LocaleManager"
import { SDLWindowManager } from "../manager/SDL2WindowStageManager"

export class BaseSDL2Ability extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    SDL2LocalManager.getInstance()
      .updateLocale();
    SDL2LocalManager.getInstance()
      .registerLocaleListener(this.context);
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    SDLWindowManager.getInstance()
      .init(windowStage);

    registerWindowRequestCallback((req: OHOSWindowRequest): void => {
      SDLWindowManager.getInstance()
        .handleWindowRequest(req);
    })

    this.onWindowStageCreateHook();
  }

  onWindowStageCreateHook(): void {
  }

  onWindowStageWillDestroy(windowStage: window.WindowStage): void {
  }

  onWindowStageDestroy(): void {
    SDLWindowManager.getInstance()
      .destroy();
  }

  onWindowStageRestore(windowStage: window.WindowStage): void {
    SDLWindowManager.getInstance()
      .init(windowStage);
  }

  onDestroy(): void | Promise<void> {
  }
  onWillForeground(): void {
  }
  onForeground(): void {
  }
  onDidForeground(): void {
  }
  onWillBackground(): void {
  }
  onBackground(): void {
  }
  onDidBackground(): void {
  }
  onMemoryLevel(level: AbilityConstant.MemoryLevel): void {
  }

  onConfigurationUpdate(newConfig: Configuration): void {
  }
}