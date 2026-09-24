import SDL2 from 'libsdl2.so'

@ComponentV2
export struct SDL2SurfaceComponent {
  private xcNode: FrameNode | null = null;

  
  aboutToAppear(): void {
    const windowId=this.getUIContext().getWindowId();
  }
  /**
   * @brief 采用 API19 的声明方式
   */
  build() {
    XComponent({
      type: XComponentType.SURFACE
    })
      .id('MainSDLSurfaceHolder')
      .onAttach(() => {
        this.xcNode = this.getUIContext()
          .getAttachedFrameNodeById('MainSDLSurfaceHolder')
        if (!this.xcNode) {
          return;
        }
        SDL2.bindNode('MainSDLSurfaceHolder', this.xcNode);
        this.xcNode = null;
      })
      .onDetach(() => {
        SDL2.unbindNode('MainSDLSurfaceHolder');
        this.xcNode = null;
      })
  }
}