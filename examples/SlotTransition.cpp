/*
 * Copyright (c) 2024 - 2025 the ThorVG project. All rights reserved.
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <thorvg_lottie.h>
#include "Example.h"

/************************************************************************/
/* ThorVG Drawing Contents                                              */
/************************************************************************/

#define THEME_COUNT 4

struct UserExample : tvgexam::Example
{
    unique_ptr<tvg::LottieAnimation> animation;
    uint32_t themeSlots[THEME_COUNT] = {0};  //slot IDs for each theme
    float transitionDuration = 1.0f;         //transition duration in seconds
    uint32_t w, h;

    bool transitioning = false;       //whether a transition is in progress
    uint32_t transitionStart = 0;     //elapsed time when transition started
    int currentTheme = 0;             //0 = original, 1-3 = themes
    int targetTheme = 0;              //theme we're transitioning to

    const char* themeNames[THEME_COUNT] = {
        "Original (dark)",
        "Light theme",
        "Ocean theme",
        "Forest theme"
    };

    bool update(tvg::Canvas* canvas, uint32_t elapsed) override
    {
        if (!animation) return false;

        //Update the animation frame
        animation->frame(animation->totalFrame() * tvgexam::progress(elapsed, animation->duration()));

        //Handle slot transition
        if (transitioning) {
            auto transitionElapsed = elapsed - transitionStart;
            auto progress = float(transitionElapsed) / (transitionDuration * 1000.0f);

            if (progress >= 1.0f) {
                progress = 1.0f;
                transitioning = false;
                currentTheme = targetTheme;
            }

            //First reset to original, then apply target theme with progress
            if (targetTheme == 0) {
                //Transitioning back to original: fade out current theme
                animation->apply(themeSlots[currentTheme], 1.0f - progress);
            } else if (currentTheme == 0) {
                //Transitioning from original to a theme
                animation->apply(themeSlots[targetTheme], progress);
            } else {
                //Transitioning between two themes: cross-fade
                //Fade out current, fade in target
                animation->apply(themeSlots[currentTheme], 1.0f - progress);
                animation->apply(themeSlots[targetTheme], progress);
            }
        }

        canvas->update();

        return true;
    }

    void startTransition(int theme)
    {
        if (transitioning || theme == currentTheme) return;
        if (theme < 0 || theme >= THEME_COUNT) return;

        //Reset current theme first if switching between non-original themes
        if (currentTheme != 0 && theme != 0) {
            animation->apply(0);  //reset all slots first
        }

        transitioning = true;
        transitionStart = elapsed;
        targetTheme = theme;
        cout << "Transition: " << themeNames[currentTheme] << " -> " << themeNames[theme] << endl;
    }

    bool keydown(tvg::Canvas* canvas, int32_t key) override
    {
        //Press '1' for Light theme
        if (key == SDLK_1) {
            startTransition(1);
            return true;
        }
        //Press '2' for Ocean theme
        if (key == SDLK_2) {
            startTransition(2);
            return true;
        }
        //Press '3' for Forest theme
        if (key == SDLK_3) {
            startTransition(3);
            return true;
        }
        //Press '0' or 'r' to reset to original theme
        if (key == SDLK_0 || key == SDLK_r) {
            if (currentTheme != 0 || transitioning) {
                transitioning = false;  //cancel any in-progress transition
                animation->apply(0);    //instant reset
                currentTheme = 0;
                targetTheme = 0;
                cout << "Reset to: " << themeNames[0] << endl;
            }
            return true;
        }
        return false;
    }

    bool content(tvg::Canvas* canvas, uint32_t w, uint32_t h) override
    {
        this->w = w;
        this->h = h;

        //Background
        auto bg = tvg::Shape::gen();
        bg->appendRect(0, 0, w, h);
        bg->fill(50, 50, 50);
        canvas->push(bg);

        //Create the Lottie animation
        animation = unique_ptr<tvg::LottieAnimation>(tvg::LottieAnimation::gen());
        auto picture = animation->picture();

        //Load the Lottie file
        if (!tvgexam::verify(picture->load(EXAMPLE_DIR"/lottie/extensions/slot12.json"))) return false;

        //Theme 1: Light theme - cream background, coral ball, bigger
        const char* theme1 = R"({
            "ball_color":{"p":{"a":0,"k":[0.9,0.3,0.2]}},
            "ball_scale":{"p":{"a":0,"k":[120,120,100]}},
            "bg_color":{"p":{"a":0,"k":[0.95,0.93,0.88,1]}}
        })";
        themeSlots[1] = animation->gen(theme1);

        //Theme 2: Ocean theme - deep blue background, cyan ball, semi-transparent, slightly smaller
        const char* theme2 = R"({
            "ball_color":{"p":{"a":0,"k":[0.2,0.8,0.9]}},
            "ball_opacity":{"p":{"a":0,"k":70}},
            "ball_scale":{"p":{"a":0,"k":[80,80,100]}},
            "bg_color":{"p":{"a":0,"k":[0.05,0.1,0.2,1]}}
        })";
        themeSlots[2] = animation->gen(theme2);

        //Theme 3: Forest theme - dark green background, lime ball, low opacity, smallest
        const char* theme3 = R"({
            "ball_color":{"p":{"a":0,"k":[0.4,0.9,0.2]}},
            "ball_opacity":{"p":{"a":0,"k":50}},
            "ball_scale":{"p":{"a":0,"k":[60,60,100]}},
            "bg_color":{"p":{"a":0,"k":[0.08,0.15,0.08,1]}}
        })";
        themeSlots[3] = animation->gen(theme3);

        //Verify all themes were created
        for (int i = 1; i < THEME_COUNT; i++) {
            if (themeSlots[i] == 0) {
                cout << "Failed to generate theme " << i << endl;
                return false;
            }
        }

        //Size and center the animation
        float pw, ph;
        picture->size(&pw, &ph);
        float scale = min(float(w) / pw, float(h) / ph) * 0.8f;
        picture->scale(scale);
        picture->translate((w - pw * scale) * 0.5f, (h - ph * scale) * 0.5f);

        canvas->push(picture);

        cout << "Slot Transition Demo" << endl;
        cout << "  Press 1: Light theme (cream bg, coral ball, 120% scale)" << endl;
        cout << "  Press 2: Ocean theme (dark blue bg, cyan ball, 70% opacity, 80% scale)" << endl;
        cout << "  Press 3: Forest theme (green bg, lime ball, 50% opacity, 60% scale)" << endl;
        cout << "  Press 0/r: Reset to original (100% scale)" << endl;

        return true;
    }
};


/************************************************************************/
/* Entry Point                                                          */
/************************************************************************/

int main(int argc, char **argv)
{
    return tvgexam::main(new UserExample, argc, argv, false, 800, 800, 0);
}
