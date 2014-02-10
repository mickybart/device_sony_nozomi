/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Not a Contribution, Apache license notifications and license are retained
 * for attribution purposes only.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define DEBUG_COPYBIT 0
#include <copybit.h>
#include <utils/Timers.h>
#include "hwc_copybit.h"
#include "comptype.h"
#include "gr.h"
#include "hwc_fbupdate.h"
#include "external.h"

namespace qhwc {

struct range {
    int current;
    int end;
};
struct region_iterator : public copybit_region_t {

    region_iterator(hwc_region_t *region) {
        mRegion = *region;
        r.end = region->numRects;
        r.current = 0;
        this->next = iterate;
    }

private:
    static int iterate(copybit_region_t const * self, copybit_rect_t* rect){
        if (!self || !rect) {
            ALOGE("iterate invalid parameters");
            return 0;
        }

        region_iterator const* me =
                                  static_cast<region_iterator const*>(self);
        if (me->r.current != me->r.end) {
            rect->l = me->mRegion.rects[me->r.current].left;
            rect->t = me->mRegion.rects[me->r.current].top;
            rect->r = me->mRegion.rects[me->r.current].right;
            rect->b = me->mRegion.rects[me->r.current].bottom;
            me->r.current++;
            return 1;
        }
        return 0;
    }

    hwc_region_t mRegion;
    mutable range r;
};

void CopyBit::reset() {
    mCSCMode = false;
    mCopyBitDraw = false;
}

bool CopyBit::canUseCopybitForYUV(hwc_context_t *ctx) {
    return true;
}

bool CopyBit::canUseCopybitForRGB(hwc_context_t *ctx,
                                        hwc_display_contents_1_t *list,
                                        int dpy) {
    int compositionType = qdutils::QCCompositionType::
                                    getInstance().getCompositionType();

    if (compositionType & qdutils::COMPOSITION_TYPE_DYN) {
        // DYN Composition:
        // use copybit, if (TotalRGBRenderArea < threashold * FB Area)
        // this is done based on perf inputs in ICS
        // TODO: Above condition needs to be re-evaluated in JB
        int fbWidth =  ctx->dpyAttr[dpy].xres;
        int fbHeight =  ctx->dpyAttr[dpy].yres;
        unsigned int fbArea = (fbWidth * fbHeight);
        unsigned int renderArea = getRGBRenderingArea(list);
            ALOGD_IF (DEBUG_COPYBIT, "%s:renderArea %u, fbArea %u",
                                  __FUNCTION__, renderArea, fbArea);
        if (renderArea < (mDynThreshold * fbArea))
            return true;
    } else if ((compositionType & qdutils::COMPOSITION_TYPE_MDP)) {
        // MDP composition, use COPYBIT always
        return true;
    } else if ((compositionType & qdutils::COMPOSITION_TYPE_C2D)) {
        // C2D composition, use COPYBIT
        return true;
    }
    return false;
}

unsigned int CopyBit::getRGBRenderingArea
                                    (const hwc_display_contents_1_t *list) {
    //Calculates total rendering area for RGB layers
    unsigned int renderArea = 0;
    unsigned int w=0, h=0;
    // Skipping last layer since FrameBuffer layer should not affect
    // which composition to choose
    for (unsigned int i=0; i<list->numHwLayers - 1; i++) {
        if (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER) {
            private_handle_t *hnd = (private_handle_t *)list->hwLayers[i].handle;
            if (hnd) {
                if (BUFFER_TYPE_UI == hnd->bufferType) {
                    getLayerResolution(&list->hwLayers[i], w, h);
                    renderArea += (w*h);
                }
            }
        }
    }
    return renderArea;
}

bool CopyBit::prepare(hwc_context_t *ctx, hwc_display_contents_1_t *list, int dpy) {

    if(mEngine == NULL) {
        // No copybit device found - cannot use copybit
        return false;
    }

    if (!ctx->mFBUpdate[dpy]->isUsed())
        return false;
        
    configure(ctx, list, dpy);

    hwc_layer_1_t *fbLayer = &list->hwLayers[list->numHwLayers - 1];
    private_handle_t *fbHnd = (private_handle_t *)fbLayer->handle;

    if ((dpy == HWC_DISPLAY_EXTERNAL) && 
         ctx->mExtDisplay->isExternalFbMode() && 
         (fbHnd->format != HAL_PIXEL_FORMAT_RGB_565))
    {
        mCSCMode = true;
    }
    

    if (mCopyBitDraw || mCSCMode) {
        //Allocate render buffers if they're not allocated
        int format;
        if (mCSCMode)
            format = HAL_PIXEL_FORMAT_RGB_565;
        else
            format = fbHnd->format;
        if (allocRenderBuffers(fbHnd->width, fbHnd->height, format) < 0) {
            mCopyBitDraw = false;
            mCSCMode = false;
            return false;
        }

        mCurRenderBufferIndex = (mCurRenderBufferIndex + 1) % NUM_RENDER_BUFFERS;
    }
    
    return true;
}

bool CopyBit::configure(hwc_context_t *ctx, hwc_display_contents_1_t *list, int dpy) {

    int compositionType = qdutils::QCCompositionType::
                                    getInstance().getCompositionType();

    if ((compositionType == qdutils::COMPOSITION_TYPE_GPU) ||
        (compositionType == qdutils::COMPOSITION_TYPE_CPU))   {
        //GPU/CPU composition, don't change layer composition type
        return true;
    }

    if(!(validateParams(ctx, list))) {
        ALOGE("%s:Invalid Params", __FUNCTION__);
        return false;
    }

    if(ctx->listStats[dpy].skipCount) {
        //GPU will be anyways used
        return false;
    }
    
    if (ctx->listStats[dpy].numAppLayers > MAX_NUM_LAYERS) {
        // Reached max layers supported by HWC.
        return false;
    }
    
    bool useCopybitForYUV = canUseCopybitForYUV(ctx);
    bool useCopybitForRGB = canUseCopybitForRGB(ctx, list, dpy);
    LayerProp *layerProp = ctx->layerProp[dpy];
    size_t fbLayerIndex = ctx->listStats[dpy].fbLayerIndex;
    hwc_layer_1_t *fbLayer = &list->hwLayers[fbLayerIndex];
    private_handle_t *fbHnd = (private_handle_t *)fbLayer->handle;

    int i;
    if (useCopybitForYUV) {
        for (i = 0; i < ctx->listStats[dpy].numAppLayers; i++) {
            if (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER) {
                private_handle_t *hnd = (private_handle_t *)list->hwLayers[i].handle;
                if (hnd && hnd->bufferType == BUFFER_TYPE_VIDEO) {
                    layerProp[i].mFlags |= HWC_COPYBIT;
                    list->hwLayers[i].compositionType = HWC_OVERLAY;
                    list->hwLayers[i].hints |= HWC_HINT_CLEAR_FB;
                    mCopyBitDraw = true;
                }
            }
        }
    }
    if (useCopybitForRGB || mCopyBitDraw) {
        for (i = 0; i < ctx->listStats[dpy].numAppLayers; i++) {
            if (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER) {
                private_handle_t *hnd = (private_handle_t *)list->hwLayers[i].handle;
                if (hnd && hnd->bufferType == BUFFER_TYPE_UI) {
                    layerProp[i].mFlags |= HWC_COPYBIT;
                    list->hwLayers[i].compositionType = HWC_OVERLAY;
                    list->hwLayers[i].hints |= HWC_HINT_CLEAR_FB;
                    mCopyBitDraw = true;
                }
            }
        }
    }
    if (!useCopybitForYUV && mCopyBitDraw) {
        for (i = 0; i < ctx->listStats[dpy].numAppLayers; i++) {
            if (list->hwLayers[i].compositionType == HWC_FRAMEBUFFER) {
                private_handle_t *hnd = (private_handle_t *)list->hwLayers[i].handle;
                if (hnd && hnd->bufferType == BUFFER_TYPE_VIDEO) {
                    layerProp[i].mFlags |= HWC_COPYBIT;
                    list->hwLayers[i].compositionType = HWC_OVERLAY;
                    list->hwLayers[i].hints |= HWC_HINT_CLEAR_FB;
                    mCopyBitDraw = true;
                }
            }
        }
    }

    return true;
}

int CopyBit::clear (private_handle_t* hnd, hwc_rect_t& rect)
{
    copybit_rect_t clear_rect = {
        rect.left, rect.top, rect.right, rect.bottom
    };

    copybit_image_t buf;
    buf.w = ALIGN(getWidth(hnd),32);
    buf.h = getHeight(hnd);
    buf.format = hnd->format;
    buf.base = (void *)hnd->base;
    buf.handle = (native_handle_t *)hnd;

    return mEngine->clear(mEngine, &buf, &clear_rect);
}

bool CopyBit::draw(hwc_context_t *ctx, hwc_display_contents_1_t *list,
                                                        int dpy, int32_t *fd) {
    if (!mCopyBitDraw && !mCSCMode)
        return false ;

    //render buffer
    private_handle_t *renderBuffer = getCurrentRenderBuffer();
    if (!renderBuffer) {
        ALOGE("%s: Render buffer layer handle is NULL", __FUNCTION__);
        return false;
    }

    //Wait for the previous frame to complete before rendering onto it
    if(mRelFd[mCurRenderBufferIndex] >= 0) {
        sync_wait(mRelFd[mCurRenderBufferIndex], 1000);
        close(mRelFd[mCurRenderBufferIndex]);
        mRelFd[mCurRenderBufferIndex] = -1;
    }

    hwc_layer_1_t *layer = &list->hwLayers[list->numHwLayers - 1];

    if(mCopyBitDraw) {
        hwc_rect_t rect;
        getNonWormholeRegion(list, rect);
        if (mCSCMode) {
            hwc_rect_t clear_rect = { 0, 0, 
                renderBuffer->width, renderBuffer->height };
            clear(renderBuffer, clear_rect);

            private_handle_t *tmpHnd;
            if (alloc_buffer(&tmpHnd, 
                    renderBuffer->width, renderBuffer->height, 
                    HAL_PIXEL_FORMAT_RGBA_8888, 0))
                return false;

            clear(tmpHnd, rect);
            if (!drawLayersUsingCopybit(ctx, list, tmpHnd, 0, dpy)) {
                free_buffer(tmpHnd);
                return false;
            }
            
            hwc_region_t region = { 1, &rect };
            if (drawUsingCopybit(renderBuffer, tmpHnd,
                    &rect, &rect, &region, 
                    0, 0, 255, HWC_BLENDING_NONE) < 0) {
                free_buffer(tmpHnd);
                return false;
            }
            free_buffer(tmpHnd);
        } else {
            clear(renderBuffer, rect);
            if (!drawLayersUsingCopybit(ctx, list, renderBuffer, 
                    layer->transform, dpy))
                return false;
        }
    } else if (mCSCMode) {
        hwc_rect_t clear_rect = { 0, 0, 
            renderBuffer->width, renderBuffer->height };
        clear(renderBuffer, clear_rect);

        if (drawUsingCopybit(renderBuffer, (private_handle_t *)layer->handle,
                    &layer->displayFrame, &layer->sourceCrop,
                    &layer->visibleRegionScreen, 
                    0, layer->transform, layer->planeAlpha, layer->blending) < 0)
            return false;
    }
        
    // Async mode
    if (mEngine->flush_get_fence(mEngine, fd) < 0)
        *fd = -1;

    return true;
}

bool CopyBit::drawLayersUsingCopybit(hwc_context_t *ctx, 
                                        hwc_display_contents_1_t *list,
                                        private_handle_t *renderBuffer, 
                                        int renderTransform, int dpy) {
    // draw layers marked for COPYBIT
    LayerProp *layerProp = ctx->layerProp[dpy];
    for (int i = 0; i < ctx->listStats[dpy].numAppLayers; i++) {
        hwc_layer_1_t *layer = &list->hwLayers[i];
        if(!(layerProp[i].mFlags & HWC_COPYBIT)) {
            ALOGD_IF(DEBUG_COPYBIT, "%s: Not Marked for copybit", __FUNCTION__);
            continue;
        }
        int ret;
        if (layer->acquireFenceFd != -1 ) {
            // Wait for acquire Fence on the App buffers.
            ret = sync_wait(layer->acquireFenceFd, 1000);
            if(ret < 0) {
                ALOGE("%s: sync_wait error!! error no = %d err str = %s",
                                    __FUNCTION__, errno, strerror(errno));
            }
            close(layer->acquireFenceFd);
            layer->acquireFenceFd = -1;
        }
        ret = drawUsingCopybit(renderBuffer, (private_handle_t *)layer->handle,
                    &layer->displayFrame, &layer->sourceCrop,
                    &layer->visibleRegionScreen, 
                    renderTransform, layer->transform,
                    layer->planeAlpha, layer->blending);
        if(ret < 0) {
            ALOGE("%s : drawLayerUsingCopybit failed", __FUNCTION__);
        }
    }

    return true;
}

int CopyBit::drawUsingCopybit(private_handle_t *dst, private_handle_t *src,
                                hwc_rect_t *dst_rect, hwc_rect_t *src_rect,
                                hwc_region_t *region, 
                                int dst_transform, int src_transform,
                                int plane_alpha, int blending)
{
    int err;

    // Set the copybit source:
    copybit_image_t srcImage;
    srcImage.w = getWidth(src);
    srcImage.h = getHeight(src);
    srcImage.format = src->format;
    srcImage.base = (void *)src->base;
    srcImage.handle = (native_handle_t *)src;
    srcImage.horiz_padding = srcImage.w - getWidth(src);
    // Initialize vertical padding to zero for now,
    // this needs to change to accomodate vertical stride
    // if needed in the future
    srcImage.vert_padding = 0;

    // Copybit source rect
    copybit_rect_t srcRect = {
        src_rect->left, src_rect->top,
        src_rect->right, src_rect->bottom
    };

    // Copybit destination rect
    copybit_rect_t dstRect = {
        dst_rect->left, dst_rect->top,
        dst_rect->right, dst_rect->bottom
    };

    // Copybit dst
    copybit_image_t dstImage;
    dstImage.w = ALIGN(dst->width, 32);
    dstImage.h = dst->height;
    dstImage.format = dst->format;
    dstImage.base = (void *)dst->base;
    dstImage.handle = (native_handle_t *)dst;
    dstImage.horiz_padding = dstImage.w - getWidth(dst);
    // Initialize vertical padding to zero for now,
    // this needs to change to accomodate vertical stride
    // if needed in the future
    dstImage.vert_padding = 0;

    int32_t dst_w = dst_rect->right - dst_rect->left;
    int32_t dst_h = dst_rect->bottom - dst_rect->top;
    int32_t src_w = src_rect->right - src_rect->left;
    int32_t src_h = src_rect->bottom - src_rect->top;
    
    // Copybit dst
    float copybitsMaxScale =
                      (float)mEngine->get(mEngine, COPYBIT_MAGNIFICATION_LIMIT);
    float copybitsMinScale =
                       (float)mEngine->get(mEngine, COPYBIT_MINIFICATION_LIMIT);

    int transform = dst_transform ^ src_transform;
    if((transform & HWC_TRANSFORM_ROT_90) && (dst_transform & HWC_TRANSFORM_ROT_90))
        transform ^= HWC_TRANSFORM_ROT_180;

    if(transform & HWC_TRANSFORM_ROT_90) {
        //swap screen width and height
        int tmp = dst_w;
        dst_w  = dst_h;
        dst_h = tmp;
    }

    private_handle_t *tmpHnd = NULL;

    if(dst_w <= 0 || dst_h <= 0 || src_w <= 0 || src_h <= 0) {
        ALOGE("%s: wrong params for dst_w=%d dst_h=%d \
            src_w=%d src_h=%d", __FUNCTION__, 
            dst_w, dst_h, src_w, src_h);
        return -1;
    }

    float dsdx = (float)dst_w/src_w;
    float dtdy = (float)dst_h/src_h;

    float scaleLimitMax = copybitsMaxScale * copybitsMaxScale;
    float scaleLimitMin = copybitsMinScale * copybitsMinScale;
    if(dsdx > scaleLimitMax ||
        dtdy > scaleLimitMax ||
        dsdx < 1/scaleLimitMin ||
        dtdy < 1/scaleLimitMin) {
        ALOGE("%s: greater than max supported size dsdx=%f dtdy=%f \
              scaleLimitMax=%f scaleLimitMin=%f", __FUNCTION__,dsdx,dtdy,
                                          scaleLimitMax,1/scaleLimitMin);
        return -1;
    }
    if(dsdx > copybitsMaxScale ||
        dtdy > copybitsMaxScale ||
        dsdx < 1/copybitsMinScale ||
        dtdy < 1/copybitsMinScale){
        // The requested scale is out of the range the hardware
        // can support.
        ALOGD("%s:%d::Need to scale twice dsdx=%f,dtdy=%f,copybitsMaxScale=%f,\
                  copybitsMinScale=%f,dst_w=%d,dst_h=%d,src_w=%d src_h=%d",
                  __FUNCTION__,__LINE__,dsdx,dtdy,copybitsMaxScale,
                  1/copybitsMinScale,dst_w,dst_h,src_w,src_h);

        //Driver makes width and height as even
        //that may cause wrong calculation of the ratio
        //in display and crop.Hence we make
        //crop width and height as even.
        src_w  = (src_w/2)*2;
        src_h = (src_h/2)*2;

        int tmp_w =  src_w;
        int tmp_h =  src_h;

        if (dsdx > copybitsMaxScale || dtdy > copybitsMaxScale ){
            tmp_w = src_w*copybitsMaxScale;
            tmp_h = src_h*copybitsMaxScale;
        }else if (dsdx < 1/copybitsMinScale ||dtdy < 1/copybitsMinScale ){
            tmp_w = src_w/copybitsMinScale;
            tmp_h = src_h/copybitsMinScale;
            tmp_w  = (tmp_w/2)*2;
            tmp_h = (tmp_h/2)*2;
        }
        ALOGE("%s:%d::tmp_w = %d,tmp_h = %d",__FUNCTION__,__LINE__,tmp_w,tmp_h);

        if (0 == alloc_buffer(&tmpHnd, tmp_w, tmp_h, src->format, 0)){
            copybit_image_t tmp_dst;
            copybit_rect_t tmp_rect;
            tmp_dst.w = tmp_w;
            tmp_dst.h = tmp_h;
            tmp_dst.format = tmpHnd->format;
            tmp_dst.handle = tmpHnd;
            tmp_dst.horiz_padding = srcImage.horiz_padding;
            tmp_dst.vert_padding = srcImage.vert_padding;
            tmp_rect.l = 0;
            tmp_rect.t = 0;
            tmp_rect.r = tmp_dst.w;
            tmp_rect.b = tmp_dst.h;
            //create one clip region
            hwc_rect tmp_hwc_rect = {0,0,tmp_rect.r,tmp_rect.b};
            hwc_region_t tmp_hwc_reg = {1,(hwc_rect_t const*)&tmp_hwc_rect};
            region_iterator tmp_it(&tmp_hwc_reg);
            mEngine->set_parameter(mEngine, COPYBIT_TRANSFORM, 0);
            mEngine->set_parameter(mEngine, COPYBIT_PLANE_ALPHA, 255);
            err = mEngine->stretch(mEngine,&tmp_dst, &srcImage, &tmp_rect,
                                        &srcRect, &tmp_it);
            if(err < 0){
                ALOGE("%s:%d::tmp copybit stretch failed",__FUNCTION__,
                                                             __LINE__);
                if(tmpHnd)
                    free_buffer(tmpHnd);
                return err;
            }
            // copy new src and src rect crop
            srcImage = tmp_dst;
            srcRect = tmp_rect;
      }
    }
    // Copybit region
    region_iterator copybitRegion(region);

    mEngine->set_parameter(mEngine, COPYBIT_FRAMEBUFFER_WIDTH,
                                          dst->width);
    mEngine->set_parameter(mEngine, COPYBIT_FRAMEBUFFER_HEIGHT,
                                          dst->height);
    mEngine->set_parameter(mEngine, COPYBIT_TRANSFORM,
                                              transform);
    mEngine->set_parameter(mEngine, COPYBIT_PLANE_ALPHA, plane_alpha);
    mEngine->set_parameter(mEngine, COPYBIT_BLEND_MODE, blending);
    mEngine->set_parameter(mEngine, COPYBIT_DITHER,
                             (dst->format == HAL_PIXEL_FORMAT_RGB_565)?
                                             COPYBIT_ENABLE : COPYBIT_DISABLE);
    mEngine->set_parameter(mEngine, COPYBIT_BLIT_TO_FRAMEBUFFER,
                                                COPYBIT_ENABLE);
    err = mEngine->stretch(mEngine, &dstImage, &srcImage, &dstRect, &srcRect,
                                                   &copybitRegion);
    mEngine->set_parameter(mEngine, COPYBIT_BLIT_TO_FRAMEBUFFER,
                                               COPYBIT_DISABLE);

    if(tmpHnd)
        free_buffer(tmpHnd);

    if(err < 0)
        ALOGE("%s: copybit stretch failed",__FUNCTION__);
    return err;
}

void CopyBit::getLayerResolution(const hwc_layer_1_t* layer,
                                 unsigned int& width, unsigned int& height)
{
    hwc_rect_t displayFrame  = layer->displayFrame;

    width = displayFrame.right - displayFrame.left;
    height = displayFrame.bottom - displayFrame.top;
}

bool CopyBit::validateParams(hwc_context_t *ctx,
                                        const hwc_display_contents_1_t *list) {
    //Validate parameters
    if (!ctx) {
        ALOGE("%s:Invalid HWC context", __FUNCTION__);
        return false;
    } else if (!list) {
        ALOGE("%s:Invalid HWC layer list", __FUNCTION__);
        return false;
    }
    return true;
}


int CopyBit::allocRenderBuffers(int w, int h, int f)
{
    int ret = 0;
    for (int i = 0; i < NUM_RENDER_BUFFERS; i++) {
        if (mRenderBuffer[i] == NULL) {
            ret = alloc_buffer(&mRenderBuffer[i],
                               w, h, f,
                               0);
        }
        if(ret < 0) {
            freeRenderBuffers();
            break;
        }
    }
    return ret;
}

void CopyBit::freeRenderBuffers()
{
    for (int i = 0; i < NUM_RENDER_BUFFERS; i++) {
        if(mRenderBuffer[i]) {
            //Since we are freeing buffer close the fence if it has a valid one.
            if(mRelFd[i] >= 0) {
                close(mRelFd[i]);
                mRelFd[i] = -1;
            }
            free_buffer(mRenderBuffer[i]);
            mRenderBuffer[i] = NULL;
        }
    }
}

private_handle_t * CopyBit::getCurrentRenderBuffer() {
    return mRenderBuffer[mCurRenderBufferIndex];
}

void CopyBit::setReleaseFd(int fd) {
    if(mCopyBitDraw || mCSCMode) {
        if(mRelFd[mCurRenderBufferIndex] >= 0)
            close(mRelFd[mCurRenderBufferIndex]);
        mRelFd[mCurRenderBufferIndex] = dup(fd);
    }
}

struct copybit_device_t* CopyBit::getCopyBitDevice() {
    return mEngine;
}

CopyBit::CopyBit():mCSCMode(false), mCopyBitDraw(false),
    mCurRenderBufferIndex(0){
    hw_module_t const *module;
    for (int i = 0; i < NUM_RENDER_BUFFERS; i++) {
        mRenderBuffer[i] = NULL;
        mRelFd[i] = -1;
    }

    char value[PROPERTY_VALUE_MAX];
    property_get("debug.hwc.dynThreshold", value, "2");
    mDynThreshold = atof(value);

    if (hw_get_module(COPYBIT_HARDWARE_MODULE_ID, &module) == 0) {
        if(copybit_open(module, &mEngine) < 0) {
            ALOGE("FATAL ERROR: copybit open failed.");
        }
    } else {
        ALOGE("FATAL ERROR: copybit hw module not found");
    }
}

CopyBit::~CopyBit()
{
    freeRenderBuffers();
    if(mEngine)
    {
        copybit_close(mEngine);
        mEngine = NULL;
    }
}
}; //namespace qhwc
