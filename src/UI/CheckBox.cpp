#include "Checkbox.h"


Checkbox::Checkbox() : x(400), y(300), size(50), checked(false) {
}

Checkbox::Checkbox(float x, float y, float size)
    : x(x), y(y), size(size), checked(false) {
}

void Checkbox::draw() const {
    graphics::Brush br;
    br.fill_opacity = 0.2f;
    br.fill_color[0] = 0.15f;
    br.fill_color[1] = 0.15f;
    br.fill_color[2] = 0.15f;
    br.outline_opacity = 1.0f;
    br.outline_color[0] = 1.0f;
    br.outline_color[1] = 1.0f;
    br.outline_color[2] = 1.0f;
    drawRect(x, y, size, size, br);

    br.fill_opacity = 0.1f;
    br.fill_color[0] = 0.0f;
    br.fill_color[1] = 0.0f;
    br.fill_color[2] = 0.0f;
    const float lilSmaller = size * 0.95;
    drawRect(x, y, lilSmaller, lilSmaller, br);

    // Border for clarity
    br.fill_opacity = 0.0f;
    br.outline_width = 1.5f;
    br.outline_color[0] = 0.8f;
    br.outline_color[1] = 0.8f;
    br.outline_color[2] = 0.8f;
    drawRect(x, y, size, size, br);

    // Draw the checkmark if checked
    if (checked) {
        br.outline_opacity = 1.0f;
        br.outline_color[0] = 0.0f;
        br.outline_color[1] = 1.0f;
        br.outline_color[2] = 0.0f;
        drawLine(x - size / 4, y, x, y + size / 4, br);
        drawLine(x, y + size / 4, x + size / 4, y - size / 4, br);
    }
}

void Checkbox::handleClick(float mouseX, float mouseY,
                           std::function<void()> onChecked,
                           std::function<void()> onUnchecked) {
    if (mouseX > x - size / 2 && mouseX < x + size / 2 &&
        mouseY > y - size / 2 && mouseY < y + size / 2) {
        checked = !checked;

        if (checked && onChecked) {
            onChecked();
        } else if (!checked && onUnchecked) {
            onUnchecked();
        }
    }
}

bool Checkbox::isChecked() const {
    return checked;
}

void Checkbox::setChecked(bool state) {
    checked = state;
}

void Checkbox::setDimensions(float x, float y) {
    this->x = x;
    this->y = y;
}
