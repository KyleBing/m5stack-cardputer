#include "M5Cardputer.h"

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.println("Ready to control!");
}


void loop() {
    M5Cardputer.update();

    // 检查是否有按键变动
    if (M5Cardputer.Keyboard.isChange()) {
        // 如果有按键被按下
        if (M5Cardputer.Keyboard.isPressed()) {
            // 通过 keysState().word 获取当前按下的字符
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
            String key;
            for (char c : status.word) {
                key += c;
            }

            M5Cardputer.Display.clear();
            M5Cardputer.Display.setCursor(0, 0);
            M5Cardputer.Display.printf("Pressed: %s\n", key.c_str());

            // 逻辑处理：如果是 'a' 则反色
            if (key == "a") {
                M5Cardputer.Display.invertDisplay(true);
            } else {
                M5Cardputer.Display.invertDisplay(false);
            }
        }
    }
}