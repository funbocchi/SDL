/* Factories the native side cannot create on its own: the native code has no access to
   the ArkTS global scope, so such objects are built here and handed over once. */

function makeNewLocalStorage(initializingProperties?: object): LocalStorage {
  return new LocalStorage(initializingProperties);
}

export function getLocalStorageFactories(): Record<string, object> {
  return {
    'makeNewLocalStorage': makeNewLocalStorage
  };
}

export function getArkTSFactories(): Record<string, object> {
  const factories: Record<string, object> = {};

  factories['LocalStorage'] = getLocalStorageFactories();
  return factories;
}
