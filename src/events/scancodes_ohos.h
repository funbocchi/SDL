/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/*
 * OHOS ArkUI key code to SDL scancode mapping table.
 *
 * Entries are in ascending ARKUI_KEYCODE_* order, because the table is indexed
 * by (key_code - OHOS_KEYCODE_KEYPAD_BASE). The ArkUI key codes inherit the
 * numbering of Android's KeyEvent, so the order deliberately does not follow a
 * keyboard layout; the group separators below are only a reading aid:
 *
 *   2000..2009  phone keypad digits      2010..2011  * and #
 *   2012..2016  D-pad                    2017..2042  letters A-Z
 *   2043..2044  , and .                  2045..2053  Alt/Shift/Tab/Space, app keys
 *   2054..2066  Enter/Backspace/punct.   2067..2084  editing and navigation
 *   2085..2089  media keys               2090..2101  F1..F12
 *   2102..2122  Num Lock and numeric keypad
 *
 * This is not a complete mapping of ArkUI key codes: the sparse codes below
 * 2000 (volume, power, brightness, ...) are handled by a switch in
 * SDL_ohoskeyboard.c, and the joystick buttons (>= 2301) belong to the game
 * controller path. Keys without an SDL equivalent map to SDL_SCANCODE_UNKNOWN,
 * which SDL drops before it reaches the application.
 *
 * Sources:
 *   - https://developer.huawei.com/consumer/cn/doc/doccenter-references/api/capi-native-key-event-h#arkui_keycode
 *   - <SDK>/native/sysroot/usr/include/arkui/native_key_event.h (ArkUI_KeyCode)
 */
#define OHOS_KEYCODE_KEYPAD_BASE 2000
#define OHOS_KEYCODE_KEYPAD_MAX  2122

/* *INDENT-OFF* */ /* clang-format off */
static const SDL_Scancode ohos_keypad_scancode_table[] = {
    /* ---------------- phone keypad: digits 0-9, * and # ---------------- */
    /* 2000 */ SDL_SCANCODE_0,              /* ARKUI_KEYCODE_0 */
    /* 2001 */ SDL_SCANCODE_1,              /* ARKUI_KEYCODE_1 */
    /* 2002 */ SDL_SCANCODE_2,              /* ARKUI_KEYCODE_2 */
    /* 2003 */ SDL_SCANCODE_3,              /* ARKUI_KEYCODE_3 */
    /* 2004 */ SDL_SCANCODE_4,              /* ARKUI_KEYCODE_4 */
    /* 2005 */ SDL_SCANCODE_5,              /* ARKUI_KEYCODE_5 */
    /* 2006 */ SDL_SCANCODE_6,              /* ARKUI_KEYCODE_6 */
    /* 2007 */ SDL_SCANCODE_7,              /* ARKUI_KEYCODE_7 */
    /* 2008 */ SDL_SCANCODE_8,              /* ARKUI_KEYCODE_8 */
    /* 2009 */ SDL_SCANCODE_9,              /* ARKUI_KEYCODE_9 */
    /* 2010 */ SDL_SCANCODE_UNKNOWN,        /* ARKUI_KEYCODE_STAR - no SDL equivalent (phone keypad '*') */
    /* 2011 */ SDL_SCANCODE_UNKNOWN,        /* ARKUI_KEYCODE_POUND - no SDL equivalent (phone keypad '#') */
    /* ---------------- D-pad ---------------- */
    /* 2012 */ SDL_SCANCODE_UP,             /* ARKUI_KEYCODE_DPAD_UP */
    /* 2013 */ SDL_SCANCODE_DOWN,           /* ARKUI_KEYCODE_DPAD_DOWN */
    /* 2014 */ SDL_SCANCODE_LEFT,           /* ARKUI_KEYCODE_DPAD_LEFT */
    /* 2015 */ SDL_SCANCODE_RIGHT,          /* ARKUI_KEYCODE_DPAD_RIGHT */
    /* 2016 */ SDL_SCANCODE_SELECT,         /* ARKUI_KEYCODE_DPAD_CENTER - D-pad center, same choice as the Android backend */
    /* ---------------- letters, and the , . keys ---------------- */
    /* 2017 */ SDL_SCANCODE_A,              /* ARKUI_KEYCODE_A */
    /* 2018 */ SDL_SCANCODE_B,              /* ARKUI_KEYCODE_B */
    /* 2019 */ SDL_SCANCODE_C,              /* ARKUI_KEYCODE_C */
    /* 2020 */ SDL_SCANCODE_D,              /* ARKUI_KEYCODE_D */
    /* 2021 */ SDL_SCANCODE_E,              /* ARKUI_KEYCODE_E */
    /* 2022 */ SDL_SCANCODE_F,              /* ARKUI_KEYCODE_F */
    /* 2023 */ SDL_SCANCODE_G,              /* ARKUI_KEYCODE_G */
    /* 2024 */ SDL_SCANCODE_H,              /* ARKUI_KEYCODE_H */
    /* 2025 */ SDL_SCANCODE_I,              /* ARKUI_KEYCODE_I */
    /* 2026 */ SDL_SCANCODE_J,              /* ARKUI_KEYCODE_J */
    /* 2027 */ SDL_SCANCODE_K,              /* ARKUI_KEYCODE_K */
    /* 2028 */ SDL_SCANCODE_L,              /* ARKUI_KEYCODE_L */
    /* 2029 */ SDL_SCANCODE_M,              /* ARKUI_KEYCODE_M */
    /* 2030 */ SDL_SCANCODE_N,              /* ARKUI_KEYCODE_N */
    /* 2031 */ SDL_SCANCODE_O,              /* ARKUI_KEYCODE_O */
    /* 2032 */ SDL_SCANCODE_P,              /* ARKUI_KEYCODE_P */
    /* 2033 */ SDL_SCANCODE_Q,              /* ARKUI_KEYCODE_Q */
    /* 2034 */ SDL_SCANCODE_R,              /* ARKUI_KEYCODE_R */
    /* 2035 */ SDL_SCANCODE_S,              /* ARKUI_KEYCODE_S */
    /* 2036 */ SDL_SCANCODE_T,              /* ARKUI_KEYCODE_T */
    /* 2037 */ SDL_SCANCODE_U,              /* ARKUI_KEYCODE_U */
    /* 2038 */ SDL_SCANCODE_V,              /* ARKUI_KEYCODE_V */
    /* 2039 */ SDL_SCANCODE_W,              /* ARKUI_KEYCODE_W */
    /* 2040 */ SDL_SCANCODE_X,              /* ARKUI_KEYCODE_X */
    /* 2041 */ SDL_SCANCODE_Y,              /* ARKUI_KEYCODE_Y */
    /* 2042 */ SDL_SCANCODE_Z,              /* ARKUI_KEYCODE_Z */
    /* 2043 */ SDL_SCANCODE_COMMA,          /* ARKUI_KEYCODE_COMMA */
    /* 2044 */ SDL_SCANCODE_PERIOD,         /* ARKUI_KEYCODE_PERIOD */
    /* ---------------- Alt, Shift, Tab, Space and the application keys ---------------- */
    /* 2045 */ SDL_SCANCODE_LALT,           /* ARKUI_KEYCODE_ALT_LEFT */
    /* 2046 */ SDL_SCANCODE_RALT,           /* ARKUI_KEYCODE_ALT_RIGHT */
    /* 2047 */ SDL_SCANCODE_LSHIFT,         /* ARKUI_KEYCODE_SHIFT_LEFT */
    /* 2048 */ SDL_SCANCODE_RSHIFT,         /* ARKUI_KEYCODE_SHIFT_RIGHT */
    /* 2049 */ SDL_SCANCODE_TAB,            /* ARKUI_KEYCODE_TAB */
    /* 2050 */ SDL_SCANCODE_SPACE,          /* ARKUI_KEYCODE_SPACE */
    /* 2051 */ SDL_SCANCODE_UNKNOWN,        /* ARKUI_KEYCODE_SYM - no SDL equivalent (SYM) */
    /* 2052 */ SDL_SCANCODE_WWW,            /* ARKUI_KEYCODE_EXPLORER */
    /* 2053 */ SDL_SCANCODE_MAIL,           /* ARKUI_KEYCODE_ENVELOPE */
    /* ---------------- Enter, Backspace and punctuation ---------------- */
    /* 2054 */ SDL_SCANCODE_RETURN,         /* ARKUI_KEYCODE_ENTER */
    /* 2055 */ SDL_SCANCODE_BACKSPACE,      /* ARKUI_KEYCODE_DEL - ArkUI DEL is the backspace key */
    /* 2056 */ SDL_SCANCODE_GRAVE,          /* ARKUI_KEYCODE_GRAVE */
    /* 2057 */ SDL_SCANCODE_MINUS,          /* ARKUI_KEYCODE_MINUS */
    /* 2058 */ SDL_SCANCODE_EQUALS,         /* ARKUI_KEYCODE_EQUALS */
    /* 2059 */ SDL_SCANCODE_LEFTBRACKET,    /* ARKUI_KEYCODE_LEFT_BRACKET */
    /* 2060 */ SDL_SCANCODE_RIGHTBRACKET,   /* ARKUI_KEYCODE_RIGHT_BRACKET */
    /* 2061 */ SDL_SCANCODE_BACKSLASH,      /* ARKUI_KEYCODE_BACKSLASH */
    /* 2062 */ SDL_SCANCODE_SEMICOLON,      /* ARKUI_KEYCODE_SEMICOLON */
    /* 2063 */ SDL_SCANCODE_APOSTROPHE,     /* ARKUI_KEYCODE_APOSTROPHE */
    /* 2064 */ SDL_SCANCODE_SLASH,          /* ARKUI_KEYCODE_SLASH */
    /* 2065 */ SDL_SCANCODE_UNKNOWN,        /* ARKUI_KEYCODE_AT - no SDL equivalent (phone keypad '@') */
    /* 2066 */ SDL_SCANCODE_UNKNOWN,        /* ARKUI_KEYCODE_PLUS - no SDL equivalent (phone keypad '+') */
    /* ---------------- editing and navigation ---------------- */
    /* 2067 */ SDL_SCANCODE_MENU,           /* ARKUI_KEYCODE_MENU */
    /* 2068 */ SDL_SCANCODE_PAGEUP,         /* ARKUI_KEYCODE_PAGE_UP */
    /* 2069 */ SDL_SCANCODE_PAGEDOWN,       /* ARKUI_KEYCODE_PAGE_DOWN */
    /* 2070 */ SDL_SCANCODE_ESCAPE,         /* ARKUI_KEYCODE_ESCAPE */
    /* 2071 */ SDL_SCANCODE_DELETE,         /* ARKUI_KEYCODE_FORWARD_DEL */
    /* 2072 */ SDL_SCANCODE_LCTRL,          /* ARKUI_KEYCODE_CTRL_LEFT */
    /* 2073 */ SDL_SCANCODE_RCTRL,          /* ARKUI_KEYCODE_CTRL_RIGHT */
    /* 2074 */ SDL_SCANCODE_CAPSLOCK,       /* ARKUI_KEYCODE_CAPS_LOCK */
    /* 2075 */ SDL_SCANCODE_SCROLLLOCK,     /* ARKUI_KEYCODE_SCROLL_LOCK */
    /* 2076 */ SDL_SCANCODE_LGUI,           /* ARKUI_KEYCODE_META_LEFT */
    /* 2077 */ SDL_SCANCODE_RGUI,           /* ARKUI_KEYCODE_META_RIGHT */
    /* 2078 */ SDL_SCANCODE_UNKNOWN,        /* ARKUI_KEYCODE_FUNCTION - no SDL equivalent (FUNCTION) */
    /* 2079 */ SDL_SCANCODE_PRINTSCREEN,    /* ARKUI_KEYCODE_SYSRQ - SYSRQ/PrintScreen */
    /* 2080 */ SDL_SCANCODE_PAUSE,          /* ARKUI_KEYCODE_BREAK - BREAK shares the Pause key */
    /* 2081 */ SDL_SCANCODE_HOME,           /* ARKUI_KEYCODE_MOVE_HOME */
    /* 2082 */ SDL_SCANCODE_END,            /* ARKUI_KEYCODE_MOVE_END */
    /* 2083 */ SDL_SCANCODE_INSERT,         /* ARKUI_KEYCODE_INSERT */
    /* 2084 */ SDL_SCANCODE_AC_FORWARD,     /* ARKUI_KEYCODE_FORWARD */
    /* ---------------- media keys ---------------- */
    /* 2085 */ SDL_SCANCODE_AUDIOPLAY,      /* ARKUI_KEYCODE_MEDIA_PLAY */
    /* 2086 */ SDL_SCANCODE_UNKNOWN,        /* ARKUI_KEYCODE_MEDIA_PAUSE - SDL has no AUDIOPAUSE scancode */
    /* 2087 */ SDL_SCANCODE_UNKNOWN,        /* ARKUI_KEYCODE_MEDIA_CLOSE - no SDL equivalent (media close) */
    /* 2088 */ SDL_SCANCODE_EJECT,          /* ARKUI_KEYCODE_MEDIA_EJECT */
    /* 2089 */ SDL_SCANCODE_UNKNOWN,        /* ARKUI_KEYCODE_MEDIA_RECORD - no SDL equivalent (media record) */
    /* ---------------- function keys ---------------- */
    /* 2090 */ SDL_SCANCODE_F1,             /* ARKUI_KEYCODE_F1 */
    /* 2091 */ SDL_SCANCODE_F2,             /* ARKUI_KEYCODE_F2 */
    /* 2092 */ SDL_SCANCODE_F3,             /* ARKUI_KEYCODE_F3 */
    /* 2093 */ SDL_SCANCODE_F4,             /* ARKUI_KEYCODE_F4 */
    /* 2094 */ SDL_SCANCODE_F5,             /* ARKUI_KEYCODE_F5 */
    /* 2095 */ SDL_SCANCODE_F6,             /* ARKUI_KEYCODE_F6 */
    /* 2096 */ SDL_SCANCODE_F7,             /* ARKUI_KEYCODE_F7 */
    /* 2097 */ SDL_SCANCODE_F8,             /* ARKUI_KEYCODE_F8 */
    /* 2098 */ SDL_SCANCODE_F9,             /* ARKUI_KEYCODE_F9 */
    /* 2099 */ SDL_SCANCODE_F10,            /* ARKUI_KEYCODE_F10 */
    /* 2100 */ SDL_SCANCODE_F11,            /* ARKUI_KEYCODE_F11 */
    /* 2101 */ SDL_SCANCODE_F12,            /* ARKUI_KEYCODE_F12 */
    /* ---------------- Num Lock and the numeric keypad ---------------- */
    /* 2102 */ SDL_SCANCODE_NUMLOCKCLEAR,   /* ARKUI_KEYCODE_NUM_LOCK */
    /* 2103 */ SDL_SCANCODE_KP_0,           /* ARKUI_KEYCODE_NUMPAD_0 */
    /* 2104 */ SDL_SCANCODE_KP_1,           /* ARKUI_KEYCODE_NUMPAD_1 */
    /* 2105 */ SDL_SCANCODE_KP_2,           /* ARKUI_KEYCODE_NUMPAD_2 */
    /* 2106 */ SDL_SCANCODE_KP_3,           /* ARKUI_KEYCODE_NUMPAD_3 */
    /* 2107 */ SDL_SCANCODE_KP_4,           /* ARKUI_KEYCODE_NUMPAD_4 */
    /* 2108 */ SDL_SCANCODE_KP_5,           /* ARKUI_KEYCODE_NUMPAD_5 */
    /* 2109 */ SDL_SCANCODE_KP_6,           /* ARKUI_KEYCODE_NUMPAD_6 */
    /* 2110 */ SDL_SCANCODE_KP_7,           /* ARKUI_KEYCODE_NUMPAD_7 */
    /* 2111 */ SDL_SCANCODE_KP_8,           /* ARKUI_KEYCODE_NUMPAD_8 */
    /* 2112 */ SDL_SCANCODE_KP_9,           /* ARKUI_KEYCODE_NUMPAD_9 */
    /* 2113 */ SDL_SCANCODE_KP_DIVIDE,      /* ARKUI_KEYCODE_NUMPAD_DIVIDE */
    /* 2114 */ SDL_SCANCODE_KP_MULTIPLY,    /* ARKUI_KEYCODE_NUMPAD_MULTIPLY */
    /* 2115 */ SDL_SCANCODE_KP_MINUS,       /* ARKUI_KEYCODE_NUMPAD_SUBTRACT */
    /* 2116 */ SDL_SCANCODE_KP_PLUS,        /* ARKUI_KEYCODE_NUMPAD_ADD */
    /* 2117 */ SDL_SCANCODE_KP_PERIOD,      /* ARKUI_KEYCODE_NUMPAD_DOT */
    /* 2118 */ SDL_SCANCODE_KP_COMMA,       /* ARKUI_KEYCODE_NUMPAD_COMMA */
    /* 2119 */ SDL_SCANCODE_KP_ENTER,       /* ARKUI_KEYCODE_NUMPAD_ENTER */
    /* 2120 */ SDL_SCANCODE_KP_EQUALS,      /* ARKUI_KEYCODE_NUMPAD_EQUALS */
    /* 2121 */ SDL_SCANCODE_KP_LEFTPAREN,   /* ARKUI_KEYCODE_NUMPAD_LEFT_PAREN */
    /* 2122 */ SDL_SCANCODE_KP_RIGHTPAREN,  /* ARKUI_KEYCODE_NUMPAD_RIGHT_PAREN */

};
/* *INDENT-ON* */ /* clang-format on */

/* vi: set ts=4 sw=4 expandtab: */
