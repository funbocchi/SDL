@ComponentV2
export struct SDL2XC {
  @Require @Param windowId: number;

  getComponentId(): string {
    return `sdl2_window_${this.windowId}`;
  }

  build() {
    XComponent({
      type: XComponentType.SURFACE
    })
      .id(this.getComponentId())
      .onAttach(() => {
        
      })
      .onDetach(() => {

      })
  }
}
