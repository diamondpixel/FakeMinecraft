#pragma once

#include <graphics.h>

class Checkbox {
private:
    float x, y;   // Position of the checkbox
    float size;   // Size of the checkbox
    bool checked; // Checkbox state

public:
    Checkbox();
    Checkbox(float x, float y, float size);
    void draw() const;
    void handleClick(float mouseX, float mouseY, std::function<void()> onChecked =nullptr , std::function<void()> onUnchecked = nullptr);
    bool isChecked() const;
    void setChecked(bool state);
    void setDimensions(float x, float y);
};
