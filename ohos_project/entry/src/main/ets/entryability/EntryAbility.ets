import { window } from '@kit.ArkUI'
import { BaseSDL2Ability, WindowStageManager } from '@roger/sdl2'

export default class EntryAbility extends BaseSDL2Ability {
  onWindowStageCreate(windowStage: window.WindowStage): void {
    WindowStageManager.getInstance().setPagePath('pages/Index');
    super.onWindowStageCreate(windowStage);
  }
}
