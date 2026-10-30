import { i18n } from '@kit.LocalizationKit'
import { common, systemConfiguration } from '@kit.AbilityKit';
import sdl2 from 'libsdl2.so'

export class SDL2LocalManager {
  private static instance: SDL2LocalManager | null = null;
  private isRegistered: boolean = false;

  private constructor() {
  }

  public static getInstance(): SDL2LocalManager {
    if (SDL2LocalManager.instance === null) {
      SDL2LocalManager.instance = new SDL2LocalManager();
    }
    return SDL2LocalManager.instance;
  }

  private configCallback: systemConfiguration.UpdatedCallback = {
    onLanguageUpdated: (language: string) => {
      this.updateLocale();
    },
    onLocaleUpdated: (locale: string) => {
      this.updateLocale();
    }
  };

  public registerLocaleListener(context: common.Context): void {
    if (this.isRegistered) {
      return;
    }
    const applicationContext = context.getApplicationContext();
    try {
      applicationContext.onSystemConfigurationUpdated(this.configCallback);
      this.isRegistered = true;
    } catch (error) {
      console.error(`register failed: ${(error as BusinessError).code}`);
    }
  }

  public unregisterLocaleListener(context: common.Context): void {
    if (!this.isRegistered) {
      return;
    }
    const applicationContext = context.getApplicationContext();
    try {
      applicationContext.offSystemConfigurationUpdated(this.configCallback);
      this.isRegistered = false;
    } catch (error) {
      console.error(`unregister failed: ${(error as BusinessError).code}`);
    }
  }

  /**
   * @brief Gets the system locale and passes it to the native side.
   * @details Uses getSystemLocalInstance to obtain an intl.Locale object, and reads the language and region properties directly from it.
   */
  public updateLocale(): void {
    const locale: Intl.Locale = i18n.System.getSystemLocaleInstance();
    const language: string = locale.language;
    const region: string = locale.region ?? "";
    // sdl2.updateLocale(language, region);
  }
}