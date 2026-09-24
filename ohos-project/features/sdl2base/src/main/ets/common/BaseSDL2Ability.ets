import { AbilityConstant, UIAbility, Want } from "@kit.AbilityKit"
import window from "@ohos.window";
import sdl2 from "libsdl2.so"


export default class BaseSDL2Ability extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    sdl2.onAppCreate();
  }

  /**
   * @brief 继承后覆写以自定义主页面
   * @returns 返回主页面文件相对路径
   */
  getMainPagePath(): string {
    return 'pages/GamePage';
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    windowStage.loadContent(this.getMainPagePath(), (err) => {

    });
  }
  onWindowStageWillDestroy(windowStage: window.WindowStage): void {

  }
  onWindowStageDestroy(): void {

  }
  onWindowStageRestore(windowStage: window.WindowStage): void {

  }
  onDestroy(): void | Promise<void> {
    sdl2.onAppDestroy();
  }
  onWillForeground(): void {

  }
  onForeground(): void {
    sdl2.onAppResume();
  }
  onDidForeground(): void {

  }
  onWillBackground(): void {

  }
  onBackground(): void {
    sdl2.onAppPause();
  }
  onDidBackground(): void {

  }
}