import { AbilityConstant, Configuration, UIAbility, Want } from '@kit.AbilityKit';
import { NodeContent, window } from '@kit.ArkUI';

/**
 * Ability lifecycle, forwarded to the native side, which creates and drives the SDL
 * window for it. The names follow the UIAbility callbacks one to one.
 */
export const handleAbilityOnCreate: (ability: UIAbility, want: Want,
  launchParam: AbilityConstant.LaunchParam, appLibrary?: string,
  arkTSFactories?: Record<string, object>) => void;

export const handleAbilityOnWindowStageCreate: (ability: UIAbility,
  windowStage: window.WindowStage) => void;

export const handleAbilityOnWindowStageWillDestroy: (ability: UIAbility,
  windowStage: window.WindowStage) => void;

export const handleAbilityOnWindowStageRestore: (ability: UIAbility,
  windowStage: window.WindowStage) => void;

export const handleAbilityOnWindowStageDestroy: (ability: UIAbility) => void;

export const handleAbilityOnWillForeground: (ability: UIAbility) => void;

export const handleAbilityOnForeground: (ability: UIAbility) => void;

export const handleAbilityOnDidForeground: (ability: UIAbility) => void;

export const handleAbilityOnWillBackground: (ability: UIAbility) => void;

export const handleAbilityOnBackground: (ability: UIAbility) => void;

export const handleAbilityOnDidBackground: (ability: UIAbility) => void;

export const handleAbilityOnPrepareToTerminate: (ability: UIAbility) => boolean;

export const handleAbilityOnPrepareToTerminateAsync: (ability: UIAbility) => Promise<boolean>;

export const handleAbilityOnConfigurationUpdate: (ability: UIAbility,
  newConfig: Configuration) => void;

export const handleAbilityOnMemoryLevel: (ability: UIAbility,
  level: AbilityConstant.MemoryLevel) => void;

export const handleAbilityOnDestroy: (ability: UIAbility) => void;

/**
 * Creates the XComponent of the given SDL window, binds a SurfaceHolder to it and adds
 * the node to the NodeContent the page provides. Called from the page aboutToAppear.
 */
export const handleNodeContent: (nodeContent: NodeContent, sdlWindowId: number) => boolean;
