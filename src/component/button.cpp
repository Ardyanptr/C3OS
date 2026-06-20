#include <Arduino.h>
#include "button.h"

void detachCallback() {
    btnUp.attachClick([]() { });
    btnUp.attachDoubleClick([]() { });
    btnUp.attachMultiClick([]() { });
    btnUp.attachLongPressStart([]() { });
    btnUp.attachLongPressStop([]() { });

    btnDown.attachClick([]() { });
    btnDown.attachDoubleClick([]() { });
    btnDown.attachMultiClick([]() { });
    btnDown.attachLongPressStart([]() { });
    btnDown.attachLongPressStop([]() { });

    btnOK.attachClick([]() { });
    btnOK.attachDoubleClick([]() { });
    btnOK.attachMultiClick([]() { });
    btnOK.attachLongPressStart([]() { });
    btnOK.attachLongPressStop([]() { });

    btnAction.attachClick([]() { });
    btnAction.attachDoubleClick([]() { });
    btnAction.attachMultiClick([]() { });
    btnAction.attachLongPressStart([]() { });
    btnAction.attachLongPressStop([]() { });
}