import { i18n } from '@kit.LocalizationKit'
import sdl2 from 'libsdl2.so'

export class SDL2LocalManager {
  private static instance: SDL2LocalManager | null = null;

  private constructor() {
  }

  public static getInstance(): SDL2LocalManager {
    if (SDL2LocalManager.instance === null) {
      SDL2LocalManager.instance = new SDL2LocalManager();
    }
    return SDL2LocalManager.instance;
  }

  /**
   * @brief Gets the system locale and passes it to the native side.
   * @details Uses getSystemLocalInstance to obtain an intl.Locale object, and reads the language and region properties directly from it.
   */
  public updateLocale(): void {
    const locale: Intl.Locale = i18n.System.getSystemLocaleInstance();
    const language: string = locale.language;
    const region: string | undefined = locale.region;
    sdl2.updateLocale(language, region);
  }
}