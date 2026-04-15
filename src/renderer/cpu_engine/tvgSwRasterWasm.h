/*
 * Copyright (c) 2021 - 2026 ThorVG project. All rights reserved.

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

#ifdef THORVG_WASM_SIMD_SUPPORT

#include <wasm_simd128.h>


static inline v128_t ALPHA_BLEND(v128_t c, v128_t a)
{
    v128_t lo = wasm_u16x8_extmul_low_u8x16(c, a);
    v128_t hi = wasm_u16x8_extmul_high_u8x16(c, a);
    lo = wasm_u16x8_shr(lo, 8);
    hi = wasm_u16x8_shr(hi, 8);
    return wasm_u8x16_narrow_i16x8(lo, hi);
}


static void wasmRasterGrayscale8(uint8_t* dst, uint8_t val, uint32_t offset, int32_t len)
{
    dst += offset;

    int32_t i = 0;
    const v128_t valVec = wasm_u8x16_splat(val);
    for (; i <= len - 16; i += 16) {
        wasm_v128_store(dst + i, valVec);
    }
    for (; i < len; i++) {
        dst[i] = val;
    }
}


static void wasmRasterPixel32(uint32_t* dst, uint32_t val, uint32_t offset, int32_t len)
{
    dst += offset;

    int32_t i = 0;
    const v128_t vectorVal = wasm_u32x4_splat(val);
    for (; i <= len - 4; i += 4) {
        wasm_v128_store(dst + i, vectorVal);
    }
    for (; i < len; i++) {
        dst[i] = val;
    }
}


static bool wasmRasterTranslucentRle(SwSurface* surface, const SwRle* rle, const RenderRegion& bbox, const RenderColor& c)
{
    const SwSpan* end;
    int32_t x, len;

    //32bit channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(c.r, c.g, c.b, c.a);
        uint32_t src;

        for (auto span = rle->fetch(bbox, &end); span < end; ++span) {
            if (!span->fetch(bbox, x, len)) continue;
            if (span->coverage < 255) src = ALPHA_BLEND(color, span->coverage);
            else src = color;

            auto dst = &surface->buf32[span->y * surface->stride + x];
            auto ialpha = IA(src);

            v128_t vSrc = wasm_u32x4_splat(src);
            v128_t vIalpha = wasm_u8x16_splat((uint8_t)ialpha);

            int32_t i = 0;
            for (; i <= len - 4; i += 4) {
                v128_t vDst = wasm_v128_load(dst + i);
                v128_t blended = ALPHA_BLEND(vDst, vIalpha);
                wasm_v128_store(dst + i, wasm_i8x16_add(vSrc, blended));
            }
            for (; i < len; i++) {
                dst[i] = src + ALPHA_BLEND(dst[i], ialpha);
            }
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        TVGLOG("SW_ENGINE", "Require Wasm Optimization, Channel Size = %d", surface->channelSize);
        uint8_t src;
        for (auto span = rle->fetch(bbox, &end); span < end; ++span) {
            if (!span->fetch(bbox, x, len)) continue;
            auto dst = &surface->buf8[span->y * surface->stride + x];
            if (span->coverage < 255) src = MULTIPLY(span->coverage, c.a);
            else src = c.a;
            auto ialpha = ~c.a;
            for (auto x = 0; x < len; ++x, ++dst) {
                *dst = src + MULTIPLY(*dst, ialpha);
            }
        }
    }
    return true;
}


static bool wasmRasterTranslucentRect(SwSurface* surface, const RenderRegion& bbox, const RenderColor& c)
{
    auto h = bbox.h();
    auto w = bbox.w();

    //32bits channels
    if (surface->channelSize == sizeof(uint32_t)) {
        auto color = surface->join(c.r, c.g, c.b, c.a);
        auto buffer = surface->buf32 + (bbox.min.y * surface->stride) + bbox.min.x;
        auto ialpha = 255 - c.a;

        v128_t vColor = wasm_u32x4_splat(color);
        v128_t vIalpha = wasm_u8x16_splat((uint8_t)ialpha);

        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];

            int32_t i = 0;
            for (; i <= (int32_t)w - 4; i += 4) {
                v128_t vDst = wasm_v128_load(dst + i);
                v128_t blended = ALPHA_BLEND(vDst, vIalpha);
                wasm_v128_store(dst + i, wasm_i8x16_add(vColor, blended));
            }
            for (; i < (int32_t)w; i++) {
                dst[i] = color + ALPHA_BLEND(dst[i], ialpha);
            }
        }
    //8bit grayscale
    } else if (surface->channelSize == sizeof(uint8_t)) {
        TVGLOG("SW_ENGINE", "Require Wasm Optimization, Channel Size = %d", surface->channelSize);
        auto buffer = surface->buf8 + (bbox.min.y * surface->stride) + bbox.min.x;
        auto ialpha = ~c.a;
        for (uint32_t y = 0; y < h; ++y) {
            auto dst = &buffer[y * surface->stride];
            for (uint32_t x = 0; x < w; ++x, ++dst) {
                *dst = c.a + MULTIPLY(*dst, ialpha);
            }
        }
    }
    return true;
}

#endif
