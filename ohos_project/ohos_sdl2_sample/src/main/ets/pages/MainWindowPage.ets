import { NodeContent } from '@kit.ArkUI'
import { handleNodeContent } from 'libSDL2.so'
import { SDL_CREATE_INFO_KEY } from '../common/SDLConstants';

/* Creation information the native side hands to this page through its LocalStorage. */
interface WindowPageCreateInfo {
  sdlWindowId: number
}

@Entry
@Component
struct WindowPage {
  private nodeContent: NodeContent = new NodeContent();

  aboutToAppear(): void {
    const localStorage = this.getUIContext().getSharedLocalStorage();
    if (localStorage === undefined) {
      return;
    }
    const createInfo = localStorage.get<WindowPageCreateInfo>(SDL_CREATE_INFO_KEY);
    if (createInfo === undefined) {
      return;
    }
    handleNodeContent(this.nodeContent, createInfo.sdlWindowId);
  }

  build() {
    Stack() {
      ContentSlot(this.nodeContent);
    }
    .width('100%')
    .height('100%')
  }
}
