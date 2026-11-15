import { AbilityConstant, UIAbility, Want } from "@kit.AbilityKit";
import { i18n } from "@kit.LocalizationKit";
import { provideArkTSObjects } from 'libSDL2.so'
import window from "@ohos.window";

export default class Sdl2SampleAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    provideArkTSObjects(this, "", "", i18n.System);
  }
}
