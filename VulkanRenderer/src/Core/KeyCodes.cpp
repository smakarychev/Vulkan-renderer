#include "rendererpch.h"
#include "KeyCodes.h"

std::string_view Key::keyCodeToString(KeyCode keyCode)
{
    switch (keyCode)
    {
    case Space: return "Space";
    case Apostrophe: return "Apostrophe";
    case Comma: return "Comma";
    case Minus: return "Minus";
    case Period: return "Period";
    case Slash: return "Slash";
    case D0: return "D0";
    case D1: return "D1";
    case D2: return "D2";
    case D3: return "D3";
    case D4: return "D4";
    case D5: return "D5";
    case D6: return "D6";
    case D7: return "D7";
    case D8: return "D8";
    case D9: return "D9";
    case Semicolon: return "Semicolon";
    case Equal: return "Equal";
    case A: return "A";
    case B: return "B";
    case C: return "C";
    case D: return "D";
    case E: return "E";
    case F: return "F";
    case G: return "G";
    case H: return "H";
    case I: return "I";
    case J: return "J";
    case K: return "K";
    case L: return "L";
    case M: return "M";
    case N: return "N";
    case O: return "O";
    case P: return "P";
    case Q: return "Q";
    case R: return "R";
    case S: return "S";
    case T: return "T";
    case U: return "U";
    case V: return "V";
    case W: return "W";
    case X: return "X";
    case Y: return "Y";
    case Z: return "Z";
    case LeftBracket: return "LeftBracket";
    case Backslash: return "Backslash";
    case RightBracket: return "RightBracket";
    case GraveAccent: return "GraveAccent";
    case World1: return "World1";
    case World2: return "World2";
    case Escape: return "Escape";
    case Enter: return "Enter";
    case Tab: return "Tab";
    case Backspace: return "Backspace";
    case Insert: return "Insert";
    case Delete: return "Delete";
    case Right: return "Right";
    case Left: return "Left";
    case Down: return "Down";
    case Up: return "Up";
    case PageUp: return "PageUp";
    case PageDown: return "PageDown";
    case Home: return "Home";
    case End: return "End";
    case CapsLock: return "CapsLock";
    case ScrollLock: return "ScrollLock";
    case NumLock: return "NumLock";
    case PrintScreen: return "PrintScreen";
    case Pause: return "Pause";
    case F1: return "F1";
    case F2: return "F2";
    case F3: return "F3";
    case F4: return "F4";
    case F5: return "F5";
    case F6: return "F6";
    case F7: return "F7";
    case F8: return "F8";
    case F9: return "F9";
    case F10: return "F10";
    case F11: return "F11";
    case F12: return "F12";
    case F13: return "F13";
    case F14: return "F14";
    case F15: return "F15";
    case F16: return "F16";
    case F17: return "F17";
    case F18: return "F18";
    case F19: return "F19";
    case F20: return "F20";
    case F21: return "F21";
    case F22: return "F22";
    case F23: return "F23";
    case F24: return "F24";
    case F25: return "F25";
    case KP0: return "KP0";
    case KP1: return "KP1";
    case KP2: return "KP2";
    case KP3: return "KP3";
    case KP4: return "KP4";
    case KP5: return "KP5";
    case KP6: return "KP6";
    case KP7: return "KP7";
    case KP8: return "KP8";
    case KP9: return "KP9";
    case KPDecimal: return "KPDecimal";
    case KPDivide: return "KPDivide";
    case KPMultiply: return "KPMultiply";
    case KPSubtract: return "KPSubtract";
    case KPAdd: return "KPAdd";
    case KPEnter: return "KPEnter";
    case KPEqual: return "KPEqual";
    case LeftShift: return "LeftShift";
    case LeftControl: return "LeftControl";
    case LeftAlt: return "LeftAlt";
    case LeftSuper: return "LeftSuper";
    case RightShift: return "RightShift";
    case RightControl: return "RightControl";
    case RightAlt: return "RightAlt";
    case RightSuper: return "RightSuper";
    case Menu: return "Menu";
    default: return "Unknown";
    }
}

std::string_view Mouse::mouseCodeToString(MouseButton mouseCode)
{
    switch (mouseCode)
    {
    case ButtonLeft: return "ButtonLeft";
    case ButtonRight: return "ButtonRight";
    case ButtonMiddle: return "ButtonMiddle";
    case Button3: return "Button3";
    case Button4: return "Button4";
    case Button5: return "Button5";
    case Button6: return "Button6";
    case Button7: return "Button7";
    default: return "Unknown";
    }
}
