import { NodeContent } from '@kit.ArkUI'
import { handleNodeContent } from 'libSDL2.so'
import { SDL_CREATE_INFO_KEY } from '../common/SDLConstants';

/* Creation information the native side hands to this page through its LocalStorage. */
interface SubWindowPageCreateInfo {
  sdlWindowId: number
}


@Entry
@Component
struct SubWindowPage {
  private nodeContent: NodeContent = new NodeContent();

  aboutToAppear(): void {
    const localStorage = this.getUIContext().getSharedLocalStorage();
    if (!localStorage) {
      return;
    }
    const createInfo = localStorage.get<SubWindowPageCreateInfo>(SDL_CREATE_INFO_KEY);
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
