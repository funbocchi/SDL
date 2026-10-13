import { window } from '@kit.ArkUI'

export class WindowStageManager {
  private static instance: WindowStageManager | null = null;
  private windowStage: window.WindowStage | undefined = undefined;
  private pagePath: string = "";

  private constructor() {
  }

  static getInstance(): WindowStageManager {
    if (WindowStageManager.instance === null) {
      WindowStageManager.instance = new WindowStageManager();
    }
    return WindowStageManager.instance;
  }

  public init(windowStage: window.WindowStage): void {
    this.windowStage = this.windowStage;
  }

  public destroy(): void {
    this.windowStage = undefined;
  }

  public setPagePath(path: string): void {
    this.pagePath = path;
  }

  public createNewPage(): void {
    if (this.windowStage === undefined) {
      console.error('loadMainPage failed: windowStage is not ready');
      return;
    }
    if (this.pagePath === "") {
      console.error('loadMainPage failed: mainPagePath is empty, call setMainPagePath first');
      return;
    }
    try {
      this.windowStage.loadContent(this.pagePath)
        .then(() => {

        })
        .catch((error: BusinessError) => {

        });
    } catch (error) {
    }
  }
}