import { attachWindowSurface, detachWindowSurface } from 'libsdl2.so'

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
        try {
          attachWindowSurface(this.xcNode, this.windowId);
        } catch (err) {
          console.error(`attachWindowSurface(${this.windowId}) failed: ${err.message}`);
        }
      })
      .onDetach(() => {
        try {
          detachWindowSurface(this.windowId);
        } catch (err) {
          console.error(`detachWindowSurface(${this.windowId}) failed: ${err.message}`);
        }
      })
  }
}
