import { AbilityConstant, Configuration, UIAbility, Want } from "@kit.AbilityKit";
import { window } from "@kit.ArkUI";
import native from "libSDL2.so";
import { getArkTSFactories } from "../common/SDLArkTSFactories";

export default class SDLAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    native.handleAbilityOnCreate(this, want, launchParam, '', getArkTSFactories());
  }
  onWindowStageCreate(windowStage: window.WindowStage): void {
    native.handleAbilityOnWindowStageCreate(this, windowStage);
  }
  onWindowStageWillDestroy(windowStage: window.WindowStage): void {
    native.handleAbilityOnWindowStageWillDestroy(this, windowStage);
  }
  onWindowStageDestroy(): void {
    native.handleAbilityOnWindowStageDestroy(this);
  }
  onWindowStageRestore(windowStage: window.WindowStage): void {
    native.handleAbilityOnWindowStageRestore(this, windowStage);
  }
  onDestroy(): void | Promise<void> {
    native.handleAbilityOnDestroy(this);
  }
  onWillForeground(): void {
    native.handleAbilityOnWillForeground(this);
  }
  onForeground(): void {
    native.handleAbilityOnForeground(this);
  }
  onDidForeground(): void {
    native.handleAbilityOnDidForeground(this);
  }
  onWillBackground(): void {
    native.handleAbilityOnWillBackground(this);
  }
  onBackground(): void {
    native.handleAbilityOnBackground(this);
  }
  onDidBackground(): void {
    native.handleAbilityOnDidBackground(this);
  }
  onConfigurationUpdate(newConfig: Configuration): void {
    native.handleAbilityOnConfigurationUpdate(this, newConfig);
  }
  onMemoryLevel(level: AbilityConstant.MemoryLevel): void {
    native.handleAbilityOnMemoryLevel(this, level);
  }
}
