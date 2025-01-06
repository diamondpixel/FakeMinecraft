#include "headers/TypeBox.h"
#include <algorithm>
#include <iostream>
#include <ostream>


TypeBox::TypeBox(float x, float y, float width, float height)
    : x(x), y(y), width(width), height(height), isActive(false), text("") {
}

void TypeBox::draw() const {
    graphics::Brush br;
    br.outline_opacity = 1.0f;
    br.fill_opacity = isActive ? 0.7f : 0.3f;
    br.fill_color[0] = 0.2f;
    br.fill_color[1] = 0.2f;
    br.fill_color[2] = 0.2f;

    // Draw the TypeBox background
    drawRect(x, y, width, height, br);

    // Draw the text
    br.fill_opacity = 1.0f;
    br.fill_color[0] = 1.0f;
    br.fill_color[1] = 1.0f;
    br.fill_color[2] = 1.0f;

    float textX = x - width / 2 + 5;
    drawText(textX, y + 5, 16, text, br);
}

void TypeBox::handleInput() {
    static std::unordered_map<int, float> keyPressTime;
    static std::unordered_map<int, bool> keyState;
    static bool enterPressed = false;

    const float initialDelay = 500.0f;
    const float spamInterval = 2.0f;

    float currentTime = graphics::getGlobalTime();

    if (getKeyState(graphics::SCANCODE_RETURN)) {
        if (!enterPressed) {
            enterPressed = true;
            isActive = !isActive;
        }
    } else {
        enterPressed = false;
    }

    if (isActive) {
        for (int scancode = 0; scancode < graphics::NUM_SCANCODES; ++scancode) {
            bool isPressed = getKeyState(static_cast<graphics::scancode_t>(scancode));

            if (isPressed) {
                if (!keyState[scancode]) {
                    keyState[scancode] = true;
                    keyPressTime[scancode] = currentTime;
                    processKey(scancode);
                } else {
                    float elapsedTime = currentTime - keyPressTime[scancode];
                    if (elapsedTime >= initialDelay &&
                        static_cast<int>((elapsedTime - initialDelay) / spamInterval) >
                        static_cast<int>((elapsedTime - initialDelay - spamInterval) / spamInterval)) {
                        processKey(scancode);
                    }
                }
            } else {
                keyState[scancode] = false;
                keyPressTime[scancode] = 0.0f;
            }
        }
    }
}

void TypeBox::processKey(int scancode) {
    float textWidth = approximateTextWidth(text, 14); // Use the approximation
    float maxTextWidth = width - 10; // Leave some padding

    if (scancode == graphics::SCANCODE_BACKSPACE && !text.empty()) {
        text.pop_back();
    }

    if (textWidth >= maxTextWidth) {
        return; // Prevent adding more characters if width limit is reached
    }

    // Handle key presses
    if (scancode >= graphics::SCANCODE_A && scancode <= graphics::SCANCODE_Z) {
        char c = 'a' + (scancode - graphics::SCANCODE_A);
        text += c;
    } else if (scancode >= graphics::SCANCODE_1 && scancode <= graphics::SCANCODE_0) {
        char c = '1' + (scancode - graphics::SCANCODE_1);
        text += c;
    } else if (scancode == graphics::SCANCODE_SPACE) {
        text += ' ';
    }
}

float TypeBox::approximateTextWidth(const std::string &text, float fontSize) {
    const float averageCharWidth = fontSize * 0.6f;
    return averageCharWidth * text.length();
}

void TypeBox::setActive(bool active) {
    isActive = active;
}

const std::string &TypeBox::getText() const {
    return text;
}

void TypeBox::clear() {
    text.clear();
}

void TypeBox::setDimensions(float x, float y) {
    this->x = x;
    this->y = y;
}
