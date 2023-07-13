/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Seeed Technology Co.,Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "el_cv.h"
#ifdef CONFIG_EL_LIB_JPEGENC
#include "JPEGENC.h"
static JPEG jpg; // static copy of JPEG encoder class
#endif

namespace edgelab {

const uint8_t RGB565_TO_RGB888_LOOKUP_TABLE_5[] = {
    0x00, 0x08, 0x10, 0x19, 0x21, 0x29, 0x31, 0x3A, 0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
    0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD, 0xC5, 0xCE, 0xD6, 0xDE, 0xE6, 0xEF, 0xF7, 0xFF,
};

const uint8_t RGB565_TO_RGB888_LOOKUP_TABLE_6[] = {
    0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2D, 0x31, 0x35, 0x39, 0x3D,
    0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D, 0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
    0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E, 0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
    0xC2, 0xC6, 0xCA, 0xCE, 0xD2, 0xD7, 0xDB, 0xDF, 0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF,
};

EL_ATTR_WEAK EL_ERR yuv422p_to_rgb(const el_img_t *src, el_img_t *dst)
{
    int32_t y;
    int32_t cr;
    int32_t cb;
    int32_t r, g, b;
    uint32_t init_index, cbcr_index, index;
    uint32_t u_chunk = src->width * src->height;
    uint32_t v_chunk = src->width * src->height + src->width * src->height / 2;
    float beta_h = (float)src->height / dst->height, beta_w = (float)src->width / dst->width;

    EL_ASSERT(src->format == EL_PIXEL_FORMAT_YUV422);

    for (int i = 0; i < dst->height; i++) {
        for (int j = 0; j < dst->width; j++) {
            int tmph = i * beta_h, tmpw = beta_w * j;
            // select pixel
            index = i * dst->width + j;
            init_index = tmph * src->width + tmpw; // ou
            cbcr_index = init_index % 2 ? init_index - 1 : init_index;

            y = src->data[init_index];
            cb = src->data[u_chunk + cbcr_index / 2];
            cr = src->data[v_chunk + cbcr_index / 2];
            r = (int32_t)(y + (14065 * (cr - 128)) / 10000);
            g = (int32_t)(y - (3455 * (cb - 128)) / 10000 - (7169 * (cr - 128)) / 10000);
            b = (int32_t)(y + (17790 * (cb - 128)) / 10000);

            switch (dst->rotate) {
                case EL_PIXEL_ROTATE_90:
                    index = (index % dst->width) * (dst->height) +
                            (dst->height - 1 - index / dst->width);
                    break;
                case EL_PIXEL_ROTATE_180:
                    index = (dst->width - 1 - index % dst->width) +
                            (dst->height - 1 - index / dst->width) * (dst->width);
                    break;
                case EL_PIXEL_ROTATE_270:
                    index =
                        (dst->width - 1 - index % dst->width) * (dst->height) + index / dst->width;
                    break;
                default:
                    break;
            }
            if (dst->format == EL_PIXEL_FORMAT_GRAYSCALE) {
                // rgb to gray
                uint8_t gray = (r * 299 + g * 587 + b * 114) / 1000;
                dst->data[index] = (uint8_t)EL_CLIP(gray, 0, 255);
            }
            else if (dst->format == EL_PIXEL_FORMAT_RGB565) {
                dst->data[index * 2 + 0] = (r & 0xF8) | (g >> 5);
                dst->data[index * 2 + 1] = ((g << 3) & 0xE0) | (b >> 3);
            }
            else if (dst->format == EL_PIXEL_FORMAT_RGB888) {
                dst->data[index * 3 + 0] = (uint8_t)EL_CLIP(r, 0, 255);
                dst->data[index * 3 + 1] = (uint8_t)EL_CLIP(g, 0, 255);
                dst->data[index * 3 + 2] = (uint8_t)EL_CLIP(b, 0, 255);
            }
        }
    }

    return EL_OK;
}

EL_ATTR_WEAK EL_ERR rgb_to_rgb(const el_img_t *src, el_img_t *dst)
{
    uint8_t r = 0, g = 0, b = 0;
    uint32_t init_index, index;
    float beta_h = (float)src->height / dst->height, beta_w = (float)src->width / dst->width;

    for (int i = 0; i < dst->height; i++) {
        for (int j = 0; j < dst->width; j++) {
            int tmph = i * beta_h, tmpw = beta_w * j;
            // select pixel
            index = i * dst->width + j;
            init_index = tmph * src->width + tmpw; // ou

            if (src->format == EL_PIXEL_FORMAT_RGB888) {
                r = src->data[init_index * 3 + 0];
                g = src->data[init_index * 3 + 1];
                b = src->data[init_index * 3 + 2];
            }
            else if (src->format == EL_PIXEL_FORMAT_RGB565) {
                r = RGB565_TO_RGB888_LOOKUP_TABLE_5[((src->data[init_index * 2] & 0xF8) >> 3)];
                g = RGB565_TO_RGB888_LOOKUP_TABLE_6[((src->data[init_index * 2] & 0x07) << 3) |
                                                    ((src->data[init_index * 2 + 1] & 0xE0) >> 5)];
                b = RGB565_TO_RGB888_LOOKUP_TABLE_5[(src->data[init_index * 2 + 1] & 0x1F)];
            }
            else if (src->format == EL_PIXEL_FORMAT_GRAYSCALE) {
                r = src->data[init_index];
                g = src->data[init_index];
                b = src->data[init_index];
            }

            switch (dst->rotate) {
                case EL_PIXEL_ROTATE_90:
                    index = (index % dst->width) * (dst->height) +
                            (dst->height - 1 - index / dst->width);
                    break;
                case EL_PIXEL_ROTATE_180:
                    index = (dst->width - 1 - index % dst->width) +
                            (dst->height - 1 - index / dst->width) * (dst->width);
                    break;
                case EL_PIXEL_ROTATE_270:
                    index =
                        (dst->width - 1 - index % dst->width) * (dst->height) + index / dst->width;
                    break;

                default:
                    break;
            }

            if (dst->format == EL_PIXEL_FORMAT_GRAYSCALE) {
                // rgb to gray
                uint8_t gray = (r * 299 + g * 587 + b * 114) / 1000;
                dst->data[index] = gray;
            }
            else if (dst->format == EL_PIXEL_FORMAT_RGB565) {
                dst->data[index * 2 + 0] = (r & 0xF8) | (g >> 5);
                dst->data[index * 2 + 1] = ((g << 3) & 0xE0) | (b >> 3);
            }
            else if (dst->format == EL_PIXEL_FORMAT_RGB888) {
                dst->data[index * 3 + 0] = r;
                dst->data[index * 3 + 1] = g;
                dst->data[index * 3 + 2] = b;
            }
        }
    }

    return EL_OK;
}

#ifdef CONFIG_EL_LIB_JPEGENC
EL_ATTR_WEAK EL_ERR rgb_to_jpeg(const el_img_t *src, el_img_t *dst)
{
    JPEGENCODE jpe;
    int rc = 0;
    EL_ERR err = EL_OK;
    int iMCUCount = 0;
    int pitch = 0;
    int bytesPerPixel = 0;
    int pixelFormat = 0;
    EL_ASSERT(src->format == EL_PIXEL_FORMAT_GRAYSCALE || src->format == EL_PIXEL_FORMAT_RGB565 ||
              src->format == EL_PIXEL_FORMAT_RGB888);
    if (src->format == EL_PIXEL_FORMAT_GRAYSCALE) {
        bytesPerPixel = 1;
        pixelFormat = JPEG_PIXEL_GRAYSCALE;
    }
    else if (src->format == EL_PIXEL_FORMAT_RGB565) {
        bytesPerPixel = 2;
        pixelFormat = JPEG_PIXEL_RGB565;
    }
    else if (src->format == EL_PIXEL_FORMAT_RGB888) {
        bytesPerPixel = 3;
        pixelFormat = JPEG_PIXEL_RGB888;
    }
    pitch = src->width * bytesPerPixel;
    rc = jpg.open(dst->data, dst->size);
    if (rc != JPEG_SUCCESS) {
        err = EL_EIO;
        goto exit;
    }
    rc = jpg.encodeBegin(
        &jpe, src->width, src->height, pixelFormat, JPEG_SUBSAMPLE_444, JPEG_Q_BEST);
    if (rc != JPEG_SUCCESS) {
        err = EL_EIO;
        goto exit;
    }
    iMCUCount = ((src->width + jpe.cx - 1) / jpe.cx) * ((src->height + jpe.cy - 1) / jpe.cy);
    for (int i = 0; i < iMCUCount && rc == JPEG_SUCCESS; i++) {
        rc = jpg.addMCU(
            &jpe, &src->data[jpe.x * bytesPerPixel + jpe.y * src->width * bytesPerPixel], pitch);
    }
    if (rc != JPEG_SUCCESS) {
        err = EL_EIO;
        goto exit;
    }
    dst->size = jpg.close();

exit:
    return err;
}

#endif

EL_ATTR_WEAK EL_ERR el_img_convert(const el_img_t *src, el_img_t *dst)
{
    if (src == nullptr || dst == nullptr) {
        return EL_EINVAL;
    }
    if (src->data == nullptr || dst->data == nullptr) {
        return EL_EINVAL;
    }

    if (src->format == EL_PIXEL_FORMAT_YUV422) {
        return yuv422p_to_rgb(src, dst);
    }
    else if (src->format == EL_PIXEL_FORMAT_RGB565 || src->format == EL_PIXEL_FORMAT_RGB888 ||
             src->format == EL_PIXEL_FORMAT_GRAYSCALE) {
        if (dst->format == EL_PIXEL_FORMAT_JPEG) {
            return rgb_to_jpeg(src, dst);
        }
        else {
            return rgb_to_rgb(src, dst);
        }
    }
    else {
        return EL_ENOTSUP;
    }

    return EL_OK;
}

EL_ATTR_WEAK void el_draw_point(el_img_t *img, uint16_t x, uint16_t y, uint32_t color)
{
    int index = x + y * img->width;
    if (index >= img->width * img->height) {
        return;
    }

    if (img->format == EL_PIXEL_FORMAT_GRAYSCALE) {
        img->data[index] = color;
    }
    else if (img->format == EL_PIXEL_FORMAT_RGB565) {
        img->data[index * 2 + 0] = color & 0xFF;
        img->data[index * 2 + 1] = color >> 8 & 0xFF;
    }
    else if (img->format == EL_PIXEL_FORMAT_RGB888) {
        img->data[index * 3 + 0] = color & 0xFF;
        img->data[index * 3 + 1] = color >> 8 & 0xFF;
        img->data[index * 3 + 2] = color >> 16 & 0xFF;
    }
}

EL_ATTR_WEAK void el_fill_rect(
    el_img_t *img, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color)
{
    int bytesPerPixel = 0;
    switch (img->format) {
        case EL_PIXEL_FORMAT_GRAYSCALE:
            bytesPerPixel = 1;
            break;
        case EL_PIXEL_FORMAT_RGB565:
            bytesPerPixel = 2;
            break;
        case EL_PIXEL_FORMAT_RGB888:
            bytesPerPixel = 3;
            break;
        default:
            return;
    }

    w = x + w > (img->width - 1) ? img->width - x - 1 : w;
    h = y + h > (img->height - 1) ? img->height - y - 1 : h;
    int32_t line_step = (img->width - w) * bytesPerPixel;
    uint8_t *data = img->data + ((x + (y * img->width)) * bytesPerPixel);
    uint8_t c0 = color >> 16;
    uint8_t c1 = color >> 8;
    uint8_t c2 = color;

    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            switch (bytesPerPixel) {
                case 1:
                    data[0] = c2;
                    data++;
                    break;
                case 2:
                    data[0] = c1;
                    data[1] = c2;
                    data += 2;
                    break;
                case 3:
                    data[0] = c0;
                    data[1] = c1;
                    data[2] = c2;
                    data += 3;
                default:
                    break;
            }
        }
        data += line_step;
    }
}

EL_ATTR_WEAK void el_draw_h_line(
    el_img_t *img, uint16_t x0, uint16_t x1, uint16_t y, uint32_t color)
{
    return el_fill_rect(img, x0, y, x1 - x0, 1, color);
}

EL_ATTR_WEAK void el_draw_v_line(
    el_img_t *img, uint16_t x, uint16_t y0, uint16_t y1, uint32_t color)
{
    return el_fill_rect(img, x, y0, 1, y1 - y0, color);
}

EL_ATTR_WEAK void el_draw_rect(el_img_t *img,
                               uint16_t x,
                               uint16_t y,
                               uint16_t w,
                               uint16_t h,
                               uint32_t color,
                               uint16_t thickness)

{
    for (int i = 0; i < thickness; i++) {
        el_draw_h_line(img, x + i, x + w - i, y + i, color);
        el_draw_h_line(img, x + i, x + w - i, y + h - i, color);
        el_draw_v_line(img, x + i, y + i, y + h - i, color);
        el_draw_v_line(img, x + w - i, y + i, y + h - i, color);
    }
}

// EL_ERR
//     el_draw_line(el_img_t* img, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t
//     color)
// {
//     el_draw_point(img, x0, y0, color);
// }

} // namespace edgelab