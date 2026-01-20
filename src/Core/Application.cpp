#include "GameObject.h"

int main() {
    GameObject& gObject = GameObject::getInstance(1280,720, "");
    gObject.init();
    return 0;
}