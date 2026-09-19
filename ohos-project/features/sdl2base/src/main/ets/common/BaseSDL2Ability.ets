import { AbilityConstant, UIAbility, Want } from "@kit.AbilityKit"
import window from "@ohos.window";
import sdl2 from "libsdl2.so"


export default class BaseSDL2Ability extends UIAbility{
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    
  }
  onWindowStageCreate(windowStage: window.WindowStage): void {
    
  }
  onWindowStageWillDestroy(windowStage: window.WindowStage): void {
    
  }
  onWindowStageDestroy(): void {
    
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
}