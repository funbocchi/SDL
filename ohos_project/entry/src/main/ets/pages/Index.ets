import { SDL2XC, WindowStageManager } from '@roger/sdl2'

@Entry({ routeName: "sdl2Page", useSharedStorage: true })
@ComponentV2
struct Index {

  @Local windowId: number = -1;

  aboutToAppear(): void {
    WindowStageManager.getInstance().setPageName("sdl2Page");
    let storage: LocalStorage | undefined = this.getUIContext()
      .getSharedLocalStorage();
    if (storage !== undefined) {
      let id: number | undefined = storage.get<number>('sdl_window_id');
      if (id !== undefined) {
        this.windowId = id;
      }
    }
  }

  build() {
    Stack() {
      SDL2XC({ windowId: this.windowId })
    }
  }
}
