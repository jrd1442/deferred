#pragma once

/**************************************************

This app draws an unanimated gltf model.

**************************************************/

#include <memory>
#include <atlbase.h>
#include <vector>
#include "renderer.h"
#include "model.h"

class App2
{
public:
    App2();
    bool init(HWND hwnd);
    void drawFrame(float time);
private:
    std::unique_ptr<Dx12Renderer> pRenderer;
    std::vector<GltfModel>        models;
};