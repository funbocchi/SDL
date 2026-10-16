import { window } from '@kit.ArkUI'
import { BusinessError } from '@kit.BasicServicesKit'
import { SDLWindowRequestParams } from 'libsdl2.so'

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
    storage.setOrCreate('sdl_window_id', req.id);
    storage.setOrCreate('sdl_window_x', req.x);
    storage.setOrCreate('sdl_window_y', req.y);
    storage.setOrCreate('sdl_window_width', req.width);
    storage.setOrCreate('sdl_window_height', req.height);
    return storage;
  }

  /* SetOption */


  public setWindowSize(id: number, width: number, height: number): void {
    this.queue.enqueue(id, async () => {
      const win = this.windowList.get(id);
      if (!win) {
        return;
      }
      try {
        await win.resize(width, height);
      } catch (err) {
      }
    });
  }

  public setWindowPosition(id: number, x: number, y: number): void {
    this.queue.enqueue(id, async () => {
      const win = this.windowList.get(id);
      if (!win) {
        return;
      }
      try {
        await win.moveWindowTo(x, y);
      } catch (err) {
      }
    })
  }

  public createNewWindow(req: SDLWindowRequestParams): void {
    if (this.windowStage === undefined) {
      console.error('WindowStageManager: windowStage is not initialized.');
      return;
    }
    if (this.pageName === "") {
      console.error('WindowStageManager: pageName is not initialzed.');
      return;
    }

    let storage = this.buildStorageByReq(req);
    if (!this.mainWindowLoaded) {
      try {
        this.windowStage.loadContentByName(this.pageName, storage)
          .then(() => {
            this.mainWindowLoaded = true;
          })
          .catch((err: BusinessError) => {
            console.error(`Failed to load content. Cause code: ${err.code}, message: ${err.message}`);
          })
      } catch (error) {
        // TODO:
      }
    } else {
      try {
        let option: window.SubWindowOptions = {
          title: req.title,
          decorEnabled: false
        }
        this.windowStage.createSubWindowWithOptions(this.getTitle(req.id), option)
          .then((win: window.Window) => {
            win.loadContentByName(this.pageName, storage)
              .then(() => {
              })
              .catch(() => {
              });
          })
          .catch(() => {

          });
      } catch (error) {
      }
    }
  }
}
