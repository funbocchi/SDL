import { display, window } from '@kit.ArkUI'
import { BusinessError } from '@kit.BasicServicesKit'
import { SDLWindowRequestParams } from 'libsdl2.so'
import { common } from '@kit.AbilityKit';

class WindowTaskQueue {
  private chains: Map<number, Promise<void>> = new Map();

  enqueue(id: number, task: () => Promise<void>): void {
    const prev = this.chains.get(id) ?? Promise.resolve();
    const next = prev.then(() => task())
      .catch((err: BusinessError) => {

        console.error(`WindowTaskQueue: window ${id} task failed, ` +
          `code: ${err.code}, message: ${err.message}`)
      });

    this.chains.set(id, next);
  }

  remove(id: number): void {
    this.chains.delete(id);
  }

  clear(): void {
    this.chains.clear();
  }
}

export class WindowStageManager {
  private static instance: WindowStageManager | null = null;
  private windowStage: window.WindowStage | undefined = undefined;
  private pageName: string = "";

  private mainWindowLoaded: boolean = false;
  private windowList: Map<number, window.Window> = new Map<number, window.Window>();

  private queue: WindowTaskQueue = new WindowTaskQueue();

  private constructor() {
  }

  static getInstance(): WindowStageManager {
    if (WindowStageManager.instance === null) {
      WindowStageManager.instance = new WindowStageManager();
    }
    return WindowStageManager.instance;
  }

  private getTitle(value: number): string {
    return `sdl_window_${value}`;
  }

  private queryDisplay(displayId: number): display.Display | null {
    try {
      return display.getDisplayByIdSync(displayId);
    } catch (err) {
      console.error(`invalid displayId: ${displayId}, code: ${err.code}, fallback to default`);
      try {
        return display.getDefaultDisplaySync();
      } catch (e) {
        console.error(`getDefaultDisplaySync failed. code: ${e.code}`);
        return null;
      }
    }
  }

  public init(windowStage: window.WindowStage): void {
    this.windowStage = windowStage;
  }

  public destroy(): void {
    this.windowStage = undefined;
  }

  public setPageName(name: string): void {
    this.pageName = name;
  }

  private buildStorageByReq(req: SDLWindowRequestParams): LocalStorage {
    let storage: LocalStorage = new LocalStorage();
    storage.setOrCreate('sdl2_window_id', req.id);
    return storage;
  }

  private checkWindowManagerReady(): void {
    if (this.windowStage === undefined) {
      throw new Error("windowStage not initialized");
    }
    if (this.pageName === "") {
      throw new Error("pageName not initialized");
    }
  }

  public async createNewWindow(req: SDLWindowRequestParams): Promise<void> {
    try {
      this.checkWindowManagerReady();

      const storage = this.buildStorageByReq(req);

      let win: window.Window;

      if (this.mainWindowLoaded) {
        let options: window.SubWindowOptions = {
          title: req.title,
          decorEnabled: !req.borderless
        }
        win = await this.windowStage!.createSubWindowWithOptions(this.getTitle(req.id), options);
        await win.loadContentByName(this.pageName, storage);
        this.windowList.set(req.id, win);
      } else {
        await this.windowStage!.loadContentByName(this.pageName, storage);
        this.mainWindowLoaded = true;
        win = await this.windowStage!.getMainWindow();
        await win.setWindowTitle(req.title);
        win.setWindowDecorVisible(!req.borderless);
        this.windowList.set(req.id, win);
      }

      await win.setResizeByDragEnabled(req.resizable);

      let x: number = (req.px_x_force || !req.allowHighDPI) ? req.x : (win.getUIContext()
        .vp2px(req.x));
      let y: number = (req.px_y_force || !req.allowHighDPI) ? req.y : (win.getUIContext()
        .vp2px(req.y));
      await this.setWindowPos(req.id, x, y);

      let w: number = (req.allowHighDPI) ? win.getUIContext()
        .vp2px(req.width) : req.width;
      let h: number = (req.allowHighDPI) ? win.getUIContext()
        .vp2px(req.height) : req.height;
      await this.setWindowSize(req.id, w, h);

      if (!req.hidden) {
        this.showWindow(req.id);
      }

    } catch (err) {
      const e = err as Error;
      console.error(`createNewWindow(${req.id}) failed: ${e.message}`);
    }
  }

  //
  public async setWindowTitle(id: number, title: string): Promise<void> {
    try {
      let win = this.windowList.get(id)!;
      await win.setWindowTitle(title);
    } catch (err) {
    }
  }
  //
  public async setWindowIcon(id: number): Promise<void> {
  }
  //
  private async setWindowPos(id: number, x: number, y: number): Promise<void> {
    try {
      let win = this.windowList.get(id)!;
      y -= (win.getWindowDecorVisible()) ? win.getUIContext()
        .vp2px(win.getWindowDecorHeight()) : 0;
      await win.moveWindowToGlobalDisplay(x, y);
    } catch (err) {
    }
  }
  //
  private async setWindowSize(id: number, w: number, h: number): Promise<void> {
    try {
      let win = this.windowList.get(id)!;
      h += (win.getWindowDecorVisible()) ? win.getUIContext()
        .vp2px(win.getWindowDecorHeight()) : 0;
      await win.resize(w, h);
    } catch (err) {
    }
  }
  //
  public async setWindowMinimumSize(id: number, w: number, h: number): Promise<void> {
  }
  //
  public async setWidnowMaximumSize(id: number, w: number, h: number): Promise<void> {
  }
  //
  public async setWindowOpacity(id: number, opacity: number): Promise<void> {
  }
  //
  public async setWindowModalFor(id: number, parent_id: number): Promise<void> {
  }
  //
  public async setWindowInputFocus(id: number): Promise<void> {
  }
  //
  public async setWindowFullscreen(id: number, fullscreen: boolean): Promise<void> {
    try {
      let win = this.windowList.get(id)!;
      if (fullscreen) {
        await win.maximize(window.MaximizePresentation.ENTER_IMMERSIVE_DISABLE_TITLE_AND_DOCK_HOVER);
      } else {
        await win.recover();
      }
    } catch (err) {
    }
  }
  //
  public async showWindow(id: number): Promise<void> {
    try {
      let win = this.windowList.get(id)!;
      await win.showWindow();
      await win.restore();
    } catch (err) {
      throw new Error("EEE");
    }
  }
  //
  public async hideWindow(id: number): Promise<void> {
    try {
      await this.minimizeWindow(id);
    } catch (err) {
    }
  }
  //
  public async raiseWindow(id: number): Promise<void> {
  }
  //
  public async maximizeWindow(id: number): Promise<void> {
    try {
      let win = this.windowList.get(id)!;
      await win.maximize(window.MaximizePresentation.EXIT_IMMERSIVE);
    } catch (err) {
    }
  }
  //
  public async minimizeWindow(id: number): Promise<void> {
    try {
      let win = this.windowList.get(id)!;
      await win.minimize();
    } catch (err) {
    }
  }
  //
  public async restoreWindow(id: number): Promise<void> {
  }
  //
  private async setWindowBordered(id: number, bordered: boolean, w: number, h: number): Promise<void> {
    try {
      let win = this.windowList.get(id)!;
      win.setWindowDecorVisible(bordered);
      await this.setWindowSize(id, w, h);
    } catch (err) {
    }
  }
  //
  private async setWindowResizable(id: number, resizable: boolean): Promise<void> {
    try {
      let win = this.windowList.get(id)!;
      await win.setResizeByDragEnabled(resizable);
    } catch (err) {
    }
  }
}
