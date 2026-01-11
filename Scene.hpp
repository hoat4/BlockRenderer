#pragma once

#include "Rasterizer.hpp"

class Scene {

    public:
    Rasterizer* rasterizer;
    Scene() {}

    public:
    virtual void update();
};