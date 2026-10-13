
@ComponentV2
export struct SDL2XC {

  build() {
    Stack() {
      XComponent({
        type: XComponentType.SURFACE
      })
        .onAttach((err) => {

        })
        .onDetach((err) => {

        })
    }
  }
}