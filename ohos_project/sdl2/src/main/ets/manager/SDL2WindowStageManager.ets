import { display, window } from '@kit.ArkUI'
import { BusinessError } from '@kit.BasicServicesKit'
import { OHOSWindowRequest, notifyWindowReady } from 'libsdl2.so'
import { OHOSWindowCommand } from '../common/OHOSWindowTypes'

class WindowTaskQueue {
  private chains: Map<number, Promise<void>> = new Map();
  private pending: Map<number, Map<number, OHOSWindowRequest>> = new Map();
  private scheduled: Map<number, Set<number>> = new Map();

  public enqueue(windowId: number, task: () => Promise<void>): void {
    const previous: Promise<void> = this.chains.get(windowId) ?? Promise.resolve();
    const current: Promise<void> = previous
      .then((): Promise<void> => task())
      .catch((err: BusinessError): void => {
        console.error(`WindowTaskQueue: window ${windowId} task failed, ` +
          `code: ${err.code}, message: ${err.message}`);
      });

    this.chains.set(windowId, current);

    current.finally((): void => {
      if (this.chains.get(windowId) === current) {
        this.chains.delete(windowId);
      }
    });
  }

  public enqueueCoalesced(windowId: number, key: number, payload: OHOSWindowRequest,
    executor: (payload: OHOSWindowRequest) => Promise<void>): void {
    let pendingForWindow: Map<number, OHOSWindowRequest> | undefined = this.pending.get(windowId);
    if (!pendingForWindow) {
      pendingForWindow = new Map<number, OHOSWindowRequest>();
      this.pending.set(windowId, pendingForWindow);
    }
    pendingForWindow.set(key, payload);

    let scheduledForWindow: Set<number> | undefined = this.scheduled.get(windowId);
    if (!scheduledForWindow) {
      scheduledForWindow = new Set<number>();
      this.scheduled.set(windowId, scheduledForWindow);
    }

    if (!scheduledForWindow.has(key)) {
      scheduledForWindow.add(key);
      this.enqueue(windowId, (): Promise<void> => this.drain(windowId, key, executor));
    }
  }

  private async drain(windowId: number, key: number,
    executor: (payload: OHOSWindowRequest) => Promise<void>): Promise<void> {
    const scheduledForWindow: Set<number> | undefined = this.scheduled.get(windowId);
    if (scheduledForWindow) {
      scheduledForWindow.delete(key);
      if (scheduledForWindow.size === 0) {
        this.scheduled.delete(windowId);
      }
    }

    const pendingForWindow: Map<number, OHOSWindowRequest> | undefined = this.pending.get(windowId);
    if (!pendingForWindow) {
      return;
    }

    const payload: OHOSWindowRequest | undefined = pendingForWindow.get(key);
    pendingForWindow.delete(key);
    if (pendingForWindow.size === 0) {
      this.pending.delete(windowId);
    }

    if (payload) {
      await executor(payload);
    }
  }

  public remove(windowId: number): void {
    this.chains.delete(windowId);
    this.pending.delete(windowId);
    this.scheduled.delete(windowId);
  }

  public clear(): void {
    this.chains.clear();
    this.pending.clear();
    this.scheduled.clear();
  }
}

export class SDLWindowManager {
  private windowStage?: window.WindowStage;
  private pageName: string = '';
  private windows: Map<number, window.Window> = new Map();
  private mainWindowId: number = -1;
  private queue: WindowTaskQueue = new WindowTaskQueue();

  private static instance: SDLWindowManager | null = null;
  static getInstance(): SDLWindowManager {
    if (SDLWindowManager.instance === null) {
      SDLWindowManager.instance = new SDLWindowManager();
    }
    return SDLWindowManager.instance;
  }

  public init(windowStage: window.WindowStage): void {
    this.windowStage = windowStage;
  }
  public destroy(): void {
    this.windowStage = undefined;
    this.windows.clear();
    this.mainWindowId = -1;
    this.queue.clear();
  }
  public setPageName(name: string): void {
    this.pageName = name;
  }
  public handleWindowRequest(request: OHOSWindowRequest): void {
    if (this.isCoalescable(request.cmd)) {
      this.queue.enqueueCoalesced(
        request.id,
        request.cmd,
        request,
        (payload: OHOSWindowRequest): Promise<void> => this.executeWindowRequest(payload)
      );
      return;
    } else {
      this.queue.enqueue(request.id, (): Promise<void> => this.executeWindowRequest(request));
    }
  }

  private isCoalescable(cmd: number): boolean {
    switch (cmd) {
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_TITLE:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_POSITION:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_SIZE:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_MINIMUM_SIZE:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_MAXIMUM_SIZE:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SHOW:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_HIDE:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_RAISE:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_MAXIMIZE:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_MINIMIZE:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_RESTORE:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_BORDERED:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_RESIZABLE:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_ALWAYS_ON_TOP:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_FULLSCREEN:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_OPACITY:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_MODAL_FOR:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_MOUSE_GRAB:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_KEYBOARD_GRAB:
        return true;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_CREATE:
      case OHOSWindowCommand.OHOS_WINDOW_CMD_DESTROY:
      default:
        return false;
    }
  }

  private async executeWindowRequest(request: OHOSWindowRequest): Promise<void> {
    switch (request.cmd) {
      case OHOSWindowCommand.OHOS_WINDOW_CMD_CREATE:
        await this.createWindow(request);
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_DESTROY:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_TITLE:
        await this.setWindowTitle(request.id, request.title);
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_ICON:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_POSITION:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_SIZE:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_MINIMUM_SIZE:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_MAXIMUM_SIZE:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SHOW:
        await this.showWindow(request.id);
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_HIDE:
        await this.hideWindow(request.id);
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_RAISE:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_MAXIMIZE:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_MINIMIZE:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_RESTORE:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_BORDERED:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_RESIZABLE:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_ALWAYS_ON_TOP:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_FULLSCREEN:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_OPACITY:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_MODAL_FOR:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_INPUT_FOCUS:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_MOUSE_RECT:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_MOUSE_GRAB:
        break;
      case OHOSWindowCommand.OHOS_WINDOW_CMD_SET_KEYBOARD_GRAB:
        break;
      default:
        break;
    }
  }

  private buildSubWindowName(value: number): string {
    return `sdl_window_${value}`;
  }

  private async createWindow(request: OHOSWindowRequest): Promise<void> {
    try {
      if (!this.windowStage || this.pageName == '') {
        throw Error("windowStage || pageName invalid");
      }
      let storage: LocalStorage = new LocalStorage();
      storage.setOrCreate<number>('sdl2_window_id', request.id);
      let win: window.Window;
      if (this.mainWindowId == -1) {
        win = await this.windowStage.getMainWindow();
        this.windows.set(request.id, win);
        await win.minimize();
        await new Promise<void>((resolve: () => void) => setTimeout(resolve, 500));
        await win.loadContentByName(this.pageName, storage);
        console.warn(`createWindow(${request.id}): content loaded`);
        this.mainWindowId = request.id;
      } else {
        let options: window.SubWindowOptions = {
          title: "undefined sdl sub window",
          decorEnabled: !request.borderless
        }
        win = await this.windowStage.createSubWindowWithOptions(this.buildSubWindowName(request.id), options);
        this.windows.set(request.id, win);
        await win.loadContentByName(this.pageName, storage);
        console.warn(`createWindow(${request.id}): content loaded`);
      }
      await this.setWindowTitle(request.id, request.title);
      await this.setWindowBordered(request.id, request.borderless, request.w, request.h, request.allowHighdpi);
      await this.setWindowPos(request.id, request.x, request.y, request.allowHighdpi, request.w, request.h);
      await this.setWindowSize(request.id, request.w, request.h, request.allowHighdpi);

      console.warn(`createWindow(${request.id}): before notifyWindowReady`);
      notifyWindowReady(request.id, win.getWindowProperties()
        .id);
      console.warn(`createWindow(${request.id}): after notifyWindowReady`);
    } catch (err) {
      console.error(`createWindow(${request.id}) failed: ${err.message}`);
    }
  }
  //
  private async setWindowTitle(id: number, title: string): Promise<void> {
    const win = this.windows.get(id);
    if (!win) {
      console.warn(`setWindowTitle: window ${id} not found, ignored`);
      return;
    }
    try {
      await win.setWindowTitle(title);
    } catch (err) {
    }
  }
  //
  private async setWindowIcon(id: number): Promise<void> {
  }
  //
  private async setWindowPos(id: number, x: number, y: number, allowHighDPI: boolean, w: number, h: number): Promise<void> {
    const win = this.windows.get(id);
    if (!win) {
      console.warn(`setWindowTitle: window ${id} not found, ignored`);
      return;
    }
    try {
      let px: number = x;
      let py: number = y;
      py -= (win.getWindowDecorVisible()) ? (win.getUIContext()
        .vp2px(win.getWindowDecorHeight())) : 0;
      if (allowHighDPI) {
        px -= (win.getUIContext()
          .vp2px(w) - w) / 2;
        py -= (win.getUIContext()
          .vp2px(h) - h) / 2;
      }
      await win.moveWindowToGlobalDisplay(px, py);
    } catch (err) {
    }
  }
  //
  private async setWindowSize(id: number, w: number, h: number, allowHighdpi: boolean): Promise<void> {
    const win = this.windows.get(id);
    if (!win) {
      console.warn(`setWindowTitle: window ${id} not found, ignored`);
      return;
    }
    try {
      let width: number = (allowHighdpi) ? win.getUIContext()
        .vp2px(w) : w;
      let height: number = (allowHighdpi) ? win.getUIContext()
        .vp2px(h) : h;
      height += (win.getWindowDecorVisible()) ? (win.getUIContext()
        .vp2px(win.getWindowDecorHeight())) : 0;
      await win.resize(width, height);
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
    } catch (err) {
    }
  }
  //
  public async showWindow(id: number): Promise<void> {
    const win = this.windows.get(id);
    if (!win) {
      console.warn(`setWindowTitle: window ${id} not found, ignored`);
      return;
    }
    try {
      if (id === this.mainWindowId) {
        await win.restore();
      } else {
        await win.showWindow();
      }
    } catch (err) {
    }
  }
  //
  public async hideWindow(id: number): Promise<void> {
    const win = this.windows.get(id);
    if (!win) {
      console.warn(`setWindowTitle: window ${id} not found, ignored`);
      return;
    }
    try {
      await win.minimize();
    } catch (err) {
    }
  }
  //
  public async raiseWindow(id: number): Promise<void> {
  }
  //
  public async maximizeWindow(id: number): Promise<void> {
    try {
    } catch (err) {
    }
  }
  //
  public async minimizeWindow(id: number): Promise<void> {
    try {
    } catch (err) {
    }
  }
  //
  public async restoreWindow(id: number): Promise<void> {
  }
  //
  private async setWindowBordered(id: number, borderless: boolean, w: number, h: number, allowHighdpi: boolean): Promise<void> {
    const win = this.windows.get(id);
    if (!win) {
      console.warn(`setWindowTitle: window ${id} not found, ignored`);
      return;
    }
    try {
      if (win.getWindowDecorVisible() == borderless) {
        win.setWindowDecorVisible(!borderless);
        await this.setWindowSize(id, w, h, allowHighdpi);
      }
    } catch (err) {
    }
  }
  //
  private async setWindowResizable(id: number, resizable: boolean): Promise<void> {
    try {
    } catch (err) {
    }
  }
}
