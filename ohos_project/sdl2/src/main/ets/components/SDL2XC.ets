import sdl2 from 'libsdl2.so'

@ComponentV2
export struct SDL2XC {
  @Require @Param windowId: number;

  private xcNode: FrameNode | null = null;

  getComponentId(): string {
    return `sdl2_window_${this.windowId}`;
  }

  build() {
    XComponent({
      type: XComponentType.SURFACE
    })
      .id(this.getComponentId())
      .onAttach(() => {
        this.xcNode = this.getUIContext()
          .getAttachedFrameNodeById(this.getComponentId());
        if (!this.xcNode) {
          return;
        }
        sdl2.bindNode(this.getComponentId(), this.xcNode);
      })
      .onDetach(() => {
        sdl2.unbindNode(this.getComponentId());
      })
  }
}
