#pragma once


class Slider {
public:
    // Slider properties
    float x, y;
    float width, height;
    float minValue, maxValue;
    float currentValue;
    bool isDragging;

    // Constructor
    Slider() = default;
    Slider(float x, float y, float width, float height, float minValue, float maxValue, float currentValue)
        : x(x), y(y), width(width), height(height), minValue(minValue), maxValue(maxValue), currentValue(currentValue),
          isDragging(false) {
    }

    // Render the slider
    void draw() const;

    // Update the slider value if dragging
    void update(float mouseX);

    // Check if mouse is over the slider handle
    bool isMouseOverHandle(float mouseX, float mouseY) const;

    // Start dragging the slider if the mouse is over the handle
    void startDragging(float mouseX, float mouseY);

    // Stop dragging
    void stopDragging();
};
