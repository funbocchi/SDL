import SDL2 from 'libsdl2.so'

@ComponentV2
export struct SDL2SurfaceComponent {
  private xcNode: FrameNode | null = null;

  build() {
    XComponent({
      type: XComponentType.SURFACE
    })
      .id('SDL2_SurfaceHolder')
      .onAttach(() => {
        this.xcNode = this.getUIContext()
          .getAttachedFrameNodeById('SDL2_SurfaceHolder')
        if (!this.xcNode) {
          return;
        }
        SDL2.bindNode('SDL2_SurfaceHolder', this.xcNode);
        this.xcNode = null;
      })
      .onDetach(() => {
        SDL2.unbindNode('SDL2_SurfaceHolder');
        this.xcNode = null;
      })
  }
}