import { AbilityConstant, Configuration, UIAbility, Want } from "@kit.AbilityKit"
import { window } from '@kit.ArkUI'
import { SDL2LocalManager } from "../manager/SDL2LocaleManager"
import { WindowStageManager } from "../manager/SDL2WindowStageManager"

export class BaseSDL2Ability extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    SDL2LocalManager.getInstance()
      .updateLocale();
    SDL2LocalManager.getInstance()
      .registerLocaleListener(this.context);
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    WindowStageManager.getInstance()
      .init(windowStage);
  }

  onWindowStageWillDestroy(windowStage: window.WindowStage): void {
  }
  
  onWindowStageDestroy(): void {
    WindowStageManager.getInstance()
      .destroy();
  }
  onWindowStageRestore(windowStage: window.WindowStage): void {
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