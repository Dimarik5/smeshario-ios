/* Copyright (c) 1996-2023 Clickteam
 *
 * This source code is part of the Android exporter for Clickteam Multimedia Fusion 2.
 *
 * Permission is hereby granted to any person obtaining a legal copy
 * of Clickteam Multimedia Fusion 2 to use or modify this source code for
 * debugging, optimizing, or customizing applications created with
 * Clickteam Multimedia Fusion 2.  Any other use of this source code is prohibited.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


#include "CRenderUtils.h"

/***
 * Viewport
 */

Viewport::Viewport()
{
    this->position = Vec2iZero;
    this->size = Vec2iZero;
}

Viewport::Viewport(Vec2i position, Vec2i size)
{
    this->position = position;
    this->size = size;
}

Viewport::Viewport(int x, int y, int width, int height)
{
    this->position = Vec2i(x,y);
    this->size = Vec2i(width, height);
}

float Viewport::aspect()
{
    return size.x/(float)size.y;
}

bool Viewport::operator==(const Viewport &rhs) const{return this->position == rhs.position && this->size == rhs.size;}
bool Viewport::operator!=(const Viewport &rhs) const{return !(*this == rhs);}


/**
*
 *  CShadersVertor
 *
*/


int CShadersVector::AddShader(CShader* shader) {
    shaderPointers.push_back(shader);
    return (int)(shaderPointers.size()-1);
}

int CShadersVector::PopShader(CShader* shader) {
    shader = shaderPointers.back();
    shaderPointers.pop_back();
    return (int)(shaderPointers.size()-1);
}

void CShadersVector::RemoveShaderByIndex(int index) {
    if (index >= 0 && index < (int)shaderPointers.size()) {
        delete shaderPointers[index];
        shaderPointers[index] = nullptr;
    }
}

void CShadersVector::RemoveAllShaders() {
    for (auto shader : shaderPointers) {
        delete shader;
    }
    shaderPointers.clear();
}

CShader* CShadersVector::SearchShaderByIndex(int index) const {
    if (index >= 0 && index < (int)shaderPointers.size()) {
        return shaderPointers[index];
    } else {
        return nullptr;
    }
}

int CShadersVector::LastPushedShaderIndex() const {
    return (int)(shaderPointers.size() - 1);
}

int CShadersVector::Size() const {
    return (int)shaderPointers.size();
}

void CShadersVector::askEffectsToForgetCache()
{
    for(CShader* shader : shaderPointers)
    {
        if(shader)
            shader->forgetCachedState();
    }
}
//**
// Utilities for render
//**
void Scissor::setScissor(GLint *sb) {
    x = sb[0];
    y = sb[1];
    w = sb[2];
    h = sb[3];
}

int SceneRender::colorRGBA() {
    return color | alpha << 24;
}

void SceneRender::reset() {
    onFrame = false;
    onLayer = false;
    onAlpha = false;
    onScreen= true;
}
