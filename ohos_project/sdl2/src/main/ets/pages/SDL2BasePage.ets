import sdl2 from 'libsdl2.so'

@Entry
@ComponentV2
struct SDL2Page {

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