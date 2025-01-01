#include <GL/glew.h>
#include "headers/Slider.h"
#include <algorithm>
#include <Graphics.h>

void Slider::draw() const {
    // Draw slider track (background)
    graphics::Brush brush;
    brush.outline_opacity = 0.0f;
    brush.fill_color[0] = 0.8f;  // Gray
    brush.fill_color[1] = 0.8f;
    brush.fill_color[2] = 0.8f;

    drawRect(x + width / 2, y, width, height, brush);

    // Draw slider handle (knob)
    float handleX = x + (currentValue - minValue) / (maxValue - minValue) * width;
    brush.fill_color[0] = 0.2f;  // Blue
    brush.fill_color[1] = 0.5f;
    brush.fill_color[2] = 1.0f;

    drawRect(handleX, y, height * 1.5f, height * 1.5f, brush);
}

// Update the slider value if dragging
void Slider::update(float mouseX) {
    if (isDragging) {
        // Clamp mouseX within slider's range
        mouseX = std::max(x, std::min(x + width, mouseX));
        currentValue = minValue + (mouseX - x) / width * (maxValue - minValue);
    }
}

bool Slider::isMouseOverHandle(float mouseX, float mouseY) const {
    float handleX = x + (currentValue - minValue) / (maxValue - minValue) * width;
    return (mouseX >= handleX - height / 2 && mouseX <= handleX + height / 2 &&
            mouseY >= y - height / 2 && mouseY <= y + height * 1.5f);
}

void Slider::startDragging(float mouseX, float mouseY) {
    if (isMouseOverHandle(mouseX, mouseY)) {
        isDragging = true;
    }
}

void Slider::stopDragging() {
    isDragging = false;
}
