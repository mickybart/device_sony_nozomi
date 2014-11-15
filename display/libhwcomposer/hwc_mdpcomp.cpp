/*
 * Copyright (C) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <math.h>
#include "hwc_mdpcomp.h"
#include <sys/ioctl.h>
#include "external.h"
#include "qdMetaData.h"
#include "mdp_version.h"
#include "hwc_fbupdate.h"
#include <overlayRotator.h>

using overlay::Rotator;
using namespace overlay::utils;
namespace ovutils = overlay::utils;

namespace qhwc {

//==============MDPComp========================================================

IdleInvalidator *MDPComp::idleInvalidator = NULL;
bool MDPComp::sIdleFallBack = false;
bool MDPComp::sDebugLogs = false;
bool MDPComp::sEnabled = false;
int MDPComp::sMaxPipesPerMixer = MAX_PIPES_PER_MIXER;

MDPComp* MDPComp::getObject(const int& width, int dpy) {
    return new MDPComp(dpy);
}

MDPComp::MDPComp(int dpy):mDpy(dpy){};

void MDPComp::dump(android::String8& buf)
{
    dumpsys_log(buf,"HWC Map for Dpy: %s \n",
                mDpy ? "\"EXTERNAL\"" : "\"PRIMARY\"");
    dumpsys_log(buf,"CURR_FRAME: layerCount:%2d    mdpCount:%2d \
                fbCount:%2d \n", mCurrentFrame.layerCount,
                mCurrentFrame.mdpCount, mCurrentFrame.fbCount);
    dumpsys_log(buf,"needsFBRedraw:%3s  pipesUsed:%2d  MaxPipesPerMixer: %d \n",
                (mCurrentFrame.needsRedraw? "YES" : "NO"),
                mCurrentFrame.mdpCount, sMaxPipesPerMixer);
    dumpsys_log(buf," ---------------------------------------------  \n");
    dumpsys_log(buf," listIdx | cached? | mdpIndex | comptype  |  Z  \n");
    dumpsys_log(buf," ---------------------------------------------  \n");
    for(int index = 0; index < mCurrentFrame.layerCount; index++ )
        dumpsys_log(buf," %7d | %7s | %8d | %9s | %2d \n",
                    index,
                    (mCurrentFrame.isFBComposed[index] ? "YES" : "NO"),
                    mCurrentFrame.layerToMDP[index],
                    (mCurrentFrame.isFBComposed[index] ?
                     (mCurrentFrame.needsRedraw ? "GLES" : "CACHE") : "MDP"),
                    (mCurrentFrame.isFBComposed[index] ? mCurrentFrame.fbZ :
    mCurrentFrame.mdpToLayer[mCurrentFrame.layerToMDP[index]].pipeInfo->zOrder));
    dumpsys_log(buf,"\n");
}

bool MDPComp::init(hwc_context_t *ctx) {

    if(!ctx) {
        ALOGE("%s: Invalid hwc context!!",__FUNCTION__);
        return false;
    }

    char property[PROPERTY_VALUE_MAX];

    sEnabled = false;
    if((property_get("persist.hwc.mdpcomp.enable", property, NULL) > 0) &&
       (!strncmp(property, "1", PROPERTY_VALUE_MAX ) ||
        (!strncasecmp(property,"true", PROPERTY_VALUE_MAX )))) {
        sEnabled = true;
    }

    sDebugLogs = false;
    if(property_get("debug.mdpcomp.logs", property, NULL) > 0) {
        if(atoi(property) != 0)
            sDebugLogs = true;
    }

    sMaxPipesPerMixer = MAX_PIPES_PER_MIXER;
    if(property_get("debug.mdpcomp.maxpermixer", property, NULL) > 0) {
        if(atoi(property) != 0)
            sMaxPipesPerMixer = atoi(property);
    }

    if(ctx->mMDP.panel != MIPI_CMD_PANEL) {
        // Idle invalidation is not necessary on command mode panels
        long idle_timeout = DEFAULT_IDLE_TIME;
        if(property_get("debug.mdpcomp.idletime", property, NULL) > 0) {
            if(atoi(property) != 0)
                idle_timeout = atoi(property);
        }

        //create Idle Invalidator only when not disabled through property
        if(idle_timeout != -1)
            idleInvalidator = IdleInvalidator::getInstance();

        if(idleInvalidator == NULL) {
            ALOGE("%s: failed to instantiate idleInvalidator object",
                  __FUNCTION__);
        } else {
            idleInvalidator->init(timeout_handler, ctx, idle_timeout);
        }
    }
    return true;
}

void MDPComp::timeout_handler(void *udata) {
    struct hwc_context_t* ctx = (struct hwc_context_t*)(udata);

    if(!ctx) {
        ALOGE("%s: received empty data in timer callback", __FUNCTION__);
        return;
    }

    if(!ctx->proc) {
        ALOGE("%s: HWC proc not registered", __FUNCTION__);
        return;
    }
    sIdleFallBack = true;
    /* Trigger SF to redraw the current frame */
    ctx->proc->invalidate(ctx->proc);
}

void MDPComp::setMDPCompLayerFlags(hwc_context_t *ctx,
                                   hwc_display_contents_1_t* list) {
    LayerProp *layerProp = ctx->layerProp[mDpy];

    for(int index = 0; index < ctx->listStats[mDpy].numAppLayers; index++) {
        hwc_layer_1_t* layer = &(list->hwLayers[index]);
        if(!mCurrentFrame.isFBComposed[index]) {
            layerProp[index].mFlags |= HWC_MDPCOMP;
            layer->compositionType = HWC_OVERLAY;
            layer->hints |= HWC_HINT_CLEAR_FB;
        } else {
            if(!mCurrentFrame.needsRedraw)
                layer->compositionType = HWC_OVERLAY;
        }
    }
}

MDPComp::FrameInfo::FrameInfo() {
    reset(0);
}

void MDPComp::FrameInfo::reset(const int& numLayers) {
    for(int i = 0 ; i < MAX_PIPES_PER_MIXER && numLayers; i++ ) {
        if(mdpToLayer[i].pipeInfo) {
            delete mdpToLayer[i].pipeInfo;
            mdpToLayer[i].pipeInfo = NULL;
            //We dont own the rotator
            mdpToLayer[i].rot = NULL;
        }
    }

    memset(&mdpToLayer, 0, sizeof(mdpToLayer));
    memset(&layerToMDP, -1, sizeof(layerToMDP));
    memset(&isFBComposed, 1, sizeof(isFBComposed));

    layerCount = numLayers;
    fbCount = numLayers;
    mdpCount = 0;
    needsRedraw = true;
    fbZ = 0;
    mdpBasePipe = ovutils::OV_INVALID;
}

void MDPComp::FrameInfo::map() {
    // populate layer and MDP maps
    int mdpIdx = 0;
    for(int idx = 0; idx < layerCount; idx++) {
        if(!isFBComposed[idx]) {
            mdpToLayer[mdpIdx].listIndex = idx;
            layerToMDP[idx] = mdpIdx++;
        }
    }
}

MDPComp::LayerCache::LayerCache() {
    reset();
}

void MDPComp::LayerCache::reset() {
    memset(&hnd, 0, sizeof(hnd));
    memset(&isFBComposed, true, sizeof(isFBComposed));
    layerCount = 0;
}

void MDPComp::LayerCache::cacheAll(hwc_display_contents_1_t* list) {
    const int numAppLayers = list->numHwLayers - 1;
    for(int i = 0; i < numAppLayers; i++) {
        hnd[i] = list->hwLayers[i].handle;
    }
}

void MDPComp::LayerCache::updateCounts(const FrameInfo& curFrame) {
    layerCount = curFrame.layerCount;
    memcpy(&isFBComposed, &curFrame.isFBComposed, sizeof(isFBComposed));
}

bool MDPComp::LayerCache::isSameFrame(const FrameInfo& curFrame,
                                      hwc_display_contents_1_t* list) {
    if(layerCount != curFrame.layerCount)
        return false;
    for(int i = 0; i < curFrame.layerCount; i++) {
        if(curFrame.isFBComposed[i] != isFBComposed[i]) {
            return false;
        }
        if(curFrame.isFBComposed[i] &&
           (hnd[i] != list->hwLayers[i].handle)){
            return false;
        }
    }
    return true;
}

bool MDPComp::isValidDimension(hwc_context_t *ctx, hwc_layer_1_t *layer) {
    const int dpy = HWC_DISPLAY_PRIMARY;
    private_handle_t *hnd = (private_handle_t *)layer->handle;

    if(!hnd) {
        ALOGE("%s: layer handle is NULL", __FUNCTION__);
        return false;
    }

    int hw_w = ctx->dpyAttr[mDpy].xres;
    int hw_h = ctx->dpyAttr[mDpy].yres;

    hwc_rect_t crop = layer->sourceCrop;
    hwc_rect_t dst = layer->displayFrame;

    if(dst.left < 0 || dst.top < 0 || dst.right > hw_w || dst.bottom > hw_h) {
       hwc_rect_t scissor = {0, 0, hw_w, hw_h };
       qhwc::calculate_crop_rects(crop, dst, scissor, layer->transform);
    }

    int crop_w;
    int crop_h;
    if (layer->transform & HWC_TRANSFORM_ROT_90) {
        crop_w = crop.bottom - crop.top;
        crop_h = crop.right - crop.left;
    } else {
        crop_w = crop.right - crop.left;
        crop_h = crop.bottom - crop.top;
    }
    int dst_w = dst.right - dst.left;
    int dst_h = dst.bottom - dst.top;
    float w_dscale = ceilf((float)crop_w / (float)dst_w);
    float h_dscale = ceilf((float)crop_h / (float)dst_h);

    //Workaround for MDP HW limitation in DSI command mode panels where
    //FPS will not go beyond 30 if buffers on RGB pipes are of width < 5

    if((crop_w < 5)||(crop_h < 5))
        return false;

    if(ctx->mMDP.version >= qdutils::MDSS_V5) {
        /* Workaround for downscales larger than 4x.
         * Will be removed once decimator block is enabled for MDSS
         */
        if(w_dscale > 4.0f || h_dscale > 4.0f)
            return false;
    } else {
        if (ctx->mMDP.version < qdutils::MDP_V4_2 && 
            (dst_w < crop_w || dst_h < crop_h) &&
            !isYuvBuffer(hnd))
            // MDP 41 do not supports RGB downscale
            return false;
        if(w_dscale > 8.0f || h_dscale > 8.0f)
            // MDP 4 supports 1/8 downscale
            return false;
    }

    return true;
}

bool MDPComp::isValidBaseLayer(hwc_context_t *ctx, hwc_layer_1_t *layer) {
	if (ctx->mMDP.version == qdutils::MDP_V4_1) {
		int hw_w = ctx->dpyAttr[mDpy].xres;
    	int hw_h = ctx->dpyAttr[mDpy].yres;

    	hwc_rect_t dst = layer->displayFrame;

		if(dst.left != 0 || dst.top != 0 || dst.right != hw_w || dst.bottom != hw_h)
			return false;
    }

    return true;
}

bool MDPComp::isSupported(hwc_context_t *ctx, hwc_display_contents_1_t* list, int i) {
	hwc_layer_1_t *layer = &list->hwLayers[i];

    if(isSkipLayer(layer)) {
        ALOGD_IF(isDebug(), "%s: skipped layer", __FUNCTION__);
        return false;
    }

    if(layer->planeAlpha < 0xFF
                     && ctx->mMDP.version >= qdutils::MDSS_V5) {
        ALOGD_IF(isDebug(), "%s: plane alpha not implemented on MDSS",
                 __FUNCTION__);
        return false;
    }

    if(isAlphaScaled(layer)
                    && ctx->mMDP.version < qdutils::MDSS_V5) {
        ALOGD_IF(isDebug(), "%s: frame needs alpha downscaling",__FUNCTION__);
        return false;
    }

    private_handle_t *hnd = (private_handle_t *)layer->handle;
    if(isYuvBuffer(hnd) ) {
        if(isSecuring(ctx, layer)) {
            ALOGD_IF(isDebug(), "%s: MDP securing is active", __FUNCTION__);
            return false;
        }
        if(layer->planeAlpha < 0xFF) {
            ALOGD_IF(isDebug(), "%s: Cannot handle YUV layer with plane alpha\
                    when sandwiched",
                    __FUNCTION__);
            return false;
        }
    } else {
        if(layer->transform & HWC_TRANSFORM_ROT_90) {
            ALOGD_IF(isDebug(), "%s: orientation involved",__FUNCTION__);
            return false;
        }
    }

    if(!isValidDimension(ctx,layer)) {
        ALOGD_IF(isDebug(), "%s: Buffer is of invalid width", __FUNCTION__);
        return false;
    }

    if(isYuvBuffer(hnd) ) {
        int numAppLayers = ctx->listStats[mDpy].numAppLayers;
        for (i++; i < numAppLayers; i++) {
            if(!isAlphaPresent(&list->hwLayers[i]))
            	continue;
            if(list->hwLayers[i].displayFrame.left >= layer->displayFrame.right ||
               list->hwLayers[i].displayFrame.right <= layer->displayFrame.left ||
               list->hwLayers[i].displayFrame.top >= layer->displayFrame.bottom ||
               list->hwLayers[i].displayFrame.bottom <= layer->displayFrame.top)
                continue;
            if(list->hwLayers[i].displayFrame.left < layer->displayFrame.left ||
               list->hwLayers[i].displayFrame.right > layer->displayFrame.right ||
               list->hwLayers[i].displayFrame.top < layer->displayFrame.top ||
               list->hwLayers[i].displayFrame.bottom > layer->displayFrame.bottom)
                return false;
        }
    }

    return true;
}

ovutils::eDest MDPComp::getMdpPipe(hwc_context_t *ctx, ePipeType type) {
    overlay::Overlay& ov = *ctx->mOverlay;
    ovutils::eDest mdp_pipe = ovutils::OV_INVALID;

    switch(type) {
    case MDPCOMP_OV_DMA:
        mdp_pipe = ov.nextPipe(ovutils::OV_MDP_PIPE_DMA, mDpy);
        if(mdp_pipe != ovutils::OV_INVALID) {
            ctx->mDMAInUse = true;
            return mdp_pipe;
        }
    case MDPCOMP_OV_ANY:
    case MDPCOMP_OV_RGB:
        mdp_pipe = ov.nextPipe(ovutils::OV_MDP_PIPE_RGB, mDpy);
        if(mdp_pipe != ovutils::OV_INVALID) {
            return mdp_pipe;
        }

        if(type == MDPCOMP_OV_RGB) {
            //Requested only for RGB pipe
            break;
        }
    case  MDPCOMP_OV_VG:
        return ov.nextPipe(ovutils::OV_MDP_PIPE_VG, mDpy);
    default:
        ALOGE("%s: Invalid pipe type",__FUNCTION__);
        return ovutils::OV_INVALID;
    };
    return ovutils::OV_INVALID;
}

bool MDPComp::isFrameDoable(hwc_context_t *ctx) {
    int numAppLayers = ctx->listStats[mDpy].numAppLayers;
    bool ret = true;

    if(!isEnabled()) {
        ALOGD_IF(isDebug(),"%s: MDP Comp. not enabled.", __FUNCTION__);
        ret = false;
    } else if(ctx->mExtDispConfiguring) {
        ALOGD_IF( isDebug(),"%s: External Display connection is pending",
                  __FUNCTION__);
        ret = false;
    }
    return ret;
}

/* Checks for conditions where all the layers marked for MDP comp cannot be
 * bypassed. On such conditions we try to bypass atleast YUV layers */
bool MDPComp::isFullFrameDoable(hwc_context_t *ctx,
                                hwc_display_contents_1_t* list){

    if(sIdleFallBack) {
        ALOGD_IF(isDebug(), "%s: Idle fallback dpy %d",__FUNCTION__, mDpy);
        return false;
    }

    if(mDpy > HWC_DISPLAY_PRIMARY){
        ALOGD_IF(isDebug(), "%s: Cannot support External display(s)",
                 __FUNCTION__);
        return false;
    }

    //If all above hard conditions are met we can do full or partial MDP comp.
    bool ret = false;
    if(fullMDPComp(ctx, list)) {
        ret = true;
    } else if (partialMDPComp(ctx, list)) {
        ret = true;
    }
    return ret;
}

bool MDPComp::fullMDPComp(hwc_context_t *ctx, hwc_display_contents_1_t* list) {
    const int numAppLayers = ctx->listStats[mDpy].numAppLayers;

    if(isSkipPresent(ctx, mDpy)) {
        ALOGD_IF(isDebug(),"%s: SKIP present: %d",
                __FUNCTION__,
                isSkipPresent(ctx, mDpy));
        return false;
    }

    if(ctx->listStats[mDpy].planeAlpha
                     && ctx->mMDP.version >= qdutils::MDSS_V5) {
        ALOGD_IF(isDebug(), "%s: plane alpha not implemented on MDSS",
                 __FUNCTION__);
        return false;
    }

    if(ctx->listStats[mDpy].needsAlphaScale
       && ctx->mMDP.version < qdutils::MDSS_V5) {
        ALOGD_IF(isDebug(), "%s: frame needs alpha downscaling",__FUNCTION__);
        return false;
    }

    for(int i = 0; i < numAppLayers; ++i) {
        if(!isSupported(ctx, list, i))
            return false;
    }

    //Setup mCurrentFrame
    mCurrentFrame.mdpCount = mCurrentFrame.layerCount;
    mCurrentFrame.fbCount = 0;
    mCurrentFrame.fbZ = -1;
    memset(&mCurrentFrame.isFBComposed, 0, sizeof(mCurrentFrame.isFBComposed));

	int baseNeeded = int(!isValidBaseLayer(ctx, &list->hwLayers[0]));

    int mdpCount = mCurrentFrame.mdpCount + baseNeeded;
    if(mdpCount > sMaxPipesPerMixer) {
        ALOGD_IF(isDebug(), "%s: Exceeds MAX_PIPES_PER_MIXER",__FUNCTION__);
        return false;
    }

    int numPipesNeeded = pipesNeeded(ctx, list) + baseNeeded;
    int availPipes = getAvailablePipes(ctx);

    if(numPipesNeeded > availPipes) {
        ALOGD_IF(isDebug(), "%s: Insufficient MDP pipes, needed %d, avail %d",
                __FUNCTION__, numPipesNeeded, availPipes);
        return false;
    }

    return true;
}

bool MDPComp::partialMDPComp(hwc_context_t *ctx, hwc_display_contents_1_t* list)
{
    int numAppLayers = ctx->listStats[mDpy].numAppLayers;
    int batchStart, batchCount;

    //Setup mCurrentFrame
    mCurrentFrame.reset(numAppLayers);
    updateLayerCache(ctx, list);
    updateYUV(ctx, list);
    updateNotSupported(ctx, list, &batchStart, &batchCount);
    batchLayers(batchStart, batchCount); //sets up fbZ also

	int baseNeeded = int(!mCurrentFrame.isFBComposed[0] && !isValidBaseLayer(ctx, &list->hwLayers[0]));

    int mdpCount = mCurrentFrame.mdpCount + baseNeeded;
    if(!mdpCount) {
        ALOGD_IF(isDebug(), "%s: no MDP pipe used",__FUNCTION__);
        return false;
    }
    if(mdpCount > (sMaxPipesPerMixer - 1)) { // -1 since FB is used
        ALOGD_IF(isDebug(), "%s: Exceeds MAX_PIPES_PER_MIXER",__FUNCTION__);
        return false;
    }

    int numPipesNeeded = pipesNeeded(ctx, list) + baseNeeded;
    int availPipes = getAvailablePipes(ctx);

    if(numPipesNeeded > availPipes) {
        ALOGD_IF(isDebug(), "%s: Insufficient MDP pipes, needed %d, avail %d",
                __FUNCTION__, numPipesNeeded, availPipes);
        return false;
    }

    if(baseNeeded && mCurrentFrame.fbCount)
        mCurrentFrame.fbZ++;

    return true;
}

bool MDPComp::isOnlyVideoDoable(hwc_context_t *ctx,
        hwc_display_contents_1_t* list){
    int numAppLayers = ctx->listStats[mDpy].numAppLayers;
    int batchStart, batchCount;

    if(!isYuvPresent(ctx, mDpy)) {
        return false;
    }

    mCurrentFrame.reset(numAppLayers);
    updateYUV(ctx, list);
    updateNotSupported(ctx, list, &batchStart, &batchCount);
    batchLayers(batchStart, batchCount); //sets up fbZ also

	int baseNeeded = int(!mCurrentFrame.isFBComposed[0] && !isValidBaseLayer(ctx, &list->hwLayers[0]));

    int mdpCount = mCurrentFrame.mdpCount + baseNeeded;
    if(!mdpCount) {
        ALOGD_IF(isDebug(), "%s: no MDP pipe used",__FUNCTION__);
        return false;
    }
    if(mdpCount > (sMaxPipesPerMixer - 1)) { // -1 since FB is used
        ALOGD_IF(isDebug(), "%s: Exceeds MAX_PIPES_PER_MIXER",__FUNCTION__);
        return false;
    }

    int numPipesNeeded = pipesNeeded(ctx, list) + baseNeeded;
    int availPipes = getAvailablePipes(ctx);

    if(numPipesNeeded > availPipes) {
        ALOGD_IF(isDebug(), "%s: Insufficient MDP pipes, needed %d, avail %d",
                __FUNCTION__, numPipesNeeded, availPipes);
        return false;
    }

    if(baseNeeded && mCurrentFrame.fbCount)
        mCurrentFrame.fbZ++;

    return true;
}

/* Checks for conditions where YUV layers cannot be bypassed */
bool MDPComp::isYUVDoable(hwc_context_t* ctx, hwc_layer_1_t* layer) {

    if(isSkipLayer(layer)) {
        ALOGE("%s: Unable to bypass skipped YUV", __FUNCTION__);
        return false;
    }

    if(isSecuring(ctx, layer)) {
        ALOGD_IF(isDebug(), "%s: MDP securing is active", __FUNCTION__);
        return false;
    }

    if(layer->planeAlpha < 0xFF) {
        ALOGD_IF(isDebug(), "%s: Cannot handle YUV layer with plane alpha\
                when sandwiched",
                __FUNCTION__);
        return false;
    }

    if(!isValidDimension(ctx, layer)) {
        ALOGD_IF(isDebug(), "%s: Buffer is of invalid width",
            __FUNCTION__);
        return false;
    }

    return true;
}

void  MDPComp::batchLayers(int batchStart, int batchCount) {
    /* Idea is to keep as many contiguous non-updating(cached) layers in FB and
     * send rest of them through MDP. NEVER mark an updating layer for caching.
     * But cached ones can be marked for MDP*/
    /* All or Nothing is cached. No batching needed */

    if(!mCurrentFrame.fbCount) {
        mCurrentFrame.fbZ = -1;
        return;
    }
    if(!mCurrentFrame.mdpCount) {
        mCurrentFrame.fbZ = 0;
        return;
    }
    
    /*
    int nAvailPipes = getAvailablePipes(ctx);
    int minBatchStart = -1;
    int minBatchCount = mCurrentFrame.layerCount - nAvailPipes;
    int minBatchArea = 0;
    if (minBatchCount <= batchCount) {
        minBatchStart = batchStart;
        minBatchCount = batchCount;
    } else if (batchCount) {
        for (i = batchStart - (minBatchCount - batchCount); i < batchStart + (minBatchCount - batchCount)
    } else {
        for (i = 0; i < nAvailPipes; i++) {
            int area = 0;
            for (j = 0; j < minBatchCount; j++) {
                if (!mCurrentFrame.isFBComposed[i]) {
                    area = INT_MAX;
                    break;
                }
                hwc_rect_t* dst = &list->hwLayers[i + j].displayFrame;
                int dst_w = dst->right - dst->left;
                int dst_h = dst->bottom - dst->top;
                area += dst_w * dst_h;
            }
            if (minBatchArea > area) {
                minBatchStart = i;
                minBatchArea = area;
            }
        }
    }
            
    */

    int i = 0;

    if (batchCount) {
        /* Search for max number of contiguous (cached) layers base on given batch */
        for(i = batchStart - 1; i >= 0; i--) {
            if (!mCurrentFrame.isFBComposed[i])
                break;
            batchStart--;
            batchCount++;
        }
        for(i = batchStart + batchCount; i < mCurrentFrame.layerCount; i++) {
            if (!mCurrentFrame.isFBComposed[i])
                break;
            batchCount++;
        }
    } else {
        /* Search for max number of contiguous (cached) layers */
        while(i < mCurrentFrame.layerCount) {
            int count = 0;
            while(mCurrentFrame.isFBComposed[i] && i < mCurrentFrame.layerCount) {
                count++; i++;
            }
            if(count > batchCount) {
                batchCount = count;
                batchStart = i - count;
            }
            if(i < mCurrentFrame.layerCount) i++;
        }
    }

    /* reset rest of the layers for MDP comp */
    for(i = 0; i < mCurrentFrame.layerCount; i++) {
        if(i != batchStart){
            mCurrentFrame.isFBComposed[i] = false;
        } else {
            i += batchCount;
        }
    }

    mCurrentFrame.fbZ = batchStart;
    mCurrentFrame.fbCount = batchCount;
    mCurrentFrame.mdpCount = mCurrentFrame.layerCount -
            mCurrentFrame.fbCount;

    ALOGD_IF(isDebug(),"%s: cached count: %d",__FUNCTION__,
             mCurrentFrame.fbCount);
}

void MDPComp::updateLayerCache(hwc_context_t* ctx,
                               hwc_display_contents_1_t* list) {

    int numAppLayers = ctx->listStats[mDpy].numAppLayers;
    int fbCount = 0;

    for(int i = 0; i < numAppLayers; i++) {
        if (mCachedFrame.hnd[i] == list->hwLayers[i].handle) {
            fbCount++;
            mCurrentFrame.isFBComposed[i] = true;
        } else {
            mCurrentFrame.isFBComposed[i] = false;
        }
    }

    mCurrentFrame.fbCount = fbCount;
    mCurrentFrame.mdpCount = mCurrentFrame.layerCount - fbCount;
    ALOGD_IF(isDebug(),"%s: fb count: %d",__FUNCTION__, fbCount);
}

void MDPComp::updateNotSupported(hwc_context_t* ctx,
                               hwc_display_contents_1_t* list,
                               int* batchStart, int* batchCount) {

    int numAppLayers = ctx->listStats[mDpy].numAppLayers;
    int min = -1;
    int max = -2;

    for (int i = 0; i < numAppLayers; i++) {
        if (!isSupported(ctx, list, i)) {
            if(!mCurrentFrame.isFBComposed[i]) {
                mCurrentFrame.isFBComposed[i] = true;
                mCurrentFrame.fbCount++;
            }
            if (max >= 0) {
                for (int j = i - 1; j > max; j--) {
                    if(!mCurrentFrame.isFBComposed[j]) {
                        mCurrentFrame.isFBComposed[j] = true;
                        mCurrentFrame.fbCount++;
                    }
                }
            }
            max = i;
            if (min < 0)
                min = i;
        }
    }

    *batchStart = min;
    *batchCount = max - min + 1;

    mCurrentFrame.mdpCount = mCurrentFrame.layerCount -
            mCurrentFrame.fbCount;
    ALOGD_IF(isDebug(),"%s: fb count: %d",__FUNCTION__,
             mCurrentFrame.fbCount);
}

int MDPComp::getAvailablePipes(hwc_context_t* ctx) {
    int numDMAPipes = qdutils::MDPVersion::getInstance().getDMAPipes();
    overlay::Overlay& ov = *ctx->mOverlay;

    int numAvailable = ov.availablePipes(mDpy);

    //Reserve DMA for rotator
    if(ctx->mNeedsRotator)
        numAvailable -= numDMAPipes;

    //Reserve pipe(s)for FB
    if(mCurrentFrame.fbCount)
        numAvailable -= pipesForFB();

    return numAvailable;
}

void MDPComp::updateYUV(hwc_context_t* ctx, hwc_display_contents_1_t* list) {
    int nYuvCount = ctx->listStats[mDpy].yuvCount;
    if (!nYuvCount)
        return;

    int numAvailable = getAvailablePipes(ctx);

    for(int index = 0;index < nYuvCount; index++){
        int nYuvIndex = ctx->listStats[mDpy].yuvIndices[index];
        hwc_layer_1_t* layer = &list->hwLayers[nYuvIndex];

        if(isYUVDoable(ctx, layer) && 
            (nYuvIndex < numAvailable || 
            nYuvIndex >= mCurrentFrame.layerCount - numAvailable)) {
            if(mCurrentFrame.isFBComposed[nYuvIndex]) {
                mCurrentFrame.isFBComposed[nYuvIndex] = false;
                mCurrentFrame.fbCount--;
            }
        } else {
            if(!mCurrentFrame.isFBComposed[nYuvIndex]) {
                mCurrentFrame.isFBComposed[nYuvIndex] = true;
                mCurrentFrame.fbCount++;
            }
        }
    }

    mCurrentFrame.mdpCount = mCurrentFrame.layerCount -
            mCurrentFrame.fbCount;
    ALOGD_IF(isDebug(),"%s: cached count: %d",__FUNCTION__,
             mCurrentFrame.fbCount);
}

bool MDPComp::programMDP(hwc_context_t *ctx, hwc_display_contents_1_t* list) {
    ctx->mDMAInUse = false;
    if(!allocLayerPipes(ctx, list)) {
        ALOGD_IF(isDebug(), "%s: Unable to allocate MDP pipes", __FUNCTION__);
        return false;
    }
    
    int mdpNextZOrder = 0;
    if (mCurrentFrame.mdpBasePipe != ovutils::OV_INVALID) {
    	if(configureBaseLayer(ctx, mCurrentFrame.mdpBasePipe)) {
    		ALOGD_IF(isDebug(), "%s: Failed to configure overlay for \
                         base layer",__FUNCTION__);
            return false;
    	}
    	mdpNextZOrder = 1;
    }

    bool fbBatch = false;
    for (int index = 0; index < mCurrentFrame.layerCount; index++) {
        if(!mCurrentFrame.isFBComposed[index]) {
            int mdpIndex = mCurrentFrame.layerToMDP[index];
            hwc_layer_1_t* layer = &list->hwLayers[index];

            MdpPipeInfo* cur_pipe = mCurrentFrame.mdpToLayer[mdpIndex].pipeInfo;
            cur_pipe->zOrder = mdpNextZOrder++;

            if(configure(ctx, layer, mCurrentFrame.mdpToLayer[mdpIndex]) != 0 ){
                ALOGD_IF(isDebug(), "%s: Failed to configure overlay for \
                         layer %d",__FUNCTION__, index);
                return false;
            }
        } else if(fbBatch == false) {
            mdpNextZOrder++;
            fbBatch = true;
        }
    }

    return true;
}

void MDPComp::reset(const int& numLayers, hwc_display_contents_1_t* list) {
    mCurrentFrame.reset(numLayers);
    mCachedFrame.cacheAll(list);
    mCachedFrame.updateCounts(mCurrentFrame);
}

int MDPComp::prepare(hwc_context_t *ctx, hwc_display_contents_1_t* list) {

    //reset old data
    const int numLayers = ctx->listStats[mDpy].numAppLayers;
    mCurrentFrame.reset(numLayers);

    //Hard conditions, if not met, cannot do MDP comp
    if(!isFrameDoable(ctx)) {
        ALOGD_IF( isDebug(),"%s: MDP Comp not possible for this frame",
                  __FUNCTION__);
        reset(numLayers, list);
        return -1;

    }

    //Check whether layers marked for MDP Composition is actually doable.
    if(isFullFrameDoable(ctx, list)){
        mCurrentFrame.map();
        //Configure framebuffer first if applicable
        if(mCurrentFrame.fbZ >= 0) {
            if(!ctx->mFBUpdate[mDpy]->prepare(ctx, list,
                    mCurrentFrame.fbZ)) {
                ALOGE("%s configure framebuffer failed", __func__);
                reset(numLayers, list);
                return -1;
            }
        }
        //Acquire and Program MDP pipes
        if(!programMDP(ctx, list)) {
            reset(numLayers, list);
            return -1;
        } else { //Success
            //Any change in composition types needs an FB refresh
            mCurrentFrame.needsRedraw = false;
            if(!mCachedFrame.isSameFrame(mCurrentFrame, list) ||
                     (list->flags & HWC_GEOMETRY_CHANGED) ||
                     isSkipPresent(ctx, mDpy) ||
                     (mDpy > HWC_DISPLAY_PRIMARY)) {
                mCurrentFrame.needsRedraw = true;
            }
        }
    } else if(isOnlyVideoDoable(ctx, list)) {
        //All layers marked for MDP comp cannot be bypassed.
        //Try to compose atleast YUV layers through MDP comp and let
        //all the RGB layers compose in FB
        //Destination over
        mCurrentFrame.map();

        //Configure framebuffer first if applicable
        if(mCurrentFrame.fbZ >= 0) {
            if(!ctx->mFBUpdate[mDpy]->prepare(ctx, list, mCurrentFrame.fbZ)) {
                ALOGE("%s configure framebuffer failed", __func__);
                reset(numLayers, list);
                return -1;
            }
        }
        if(!programMDP(ctx, list)) {
            reset(numLayers, list);
            return -1;
        }
    } else {
        reset(numLayers, list);
        return -1;
    }

    //UpdateLayerFlags
    setMDPCompLayerFlags(ctx, list);
    mCachedFrame.cacheAll(list);
    mCachedFrame.updateCounts(mCurrentFrame);

    if(isDebug()) {
        ALOGD("GEOMETRY change: %d", (list->flags & HWC_GEOMETRY_CHANGED));
        android::String8 sDump("");
        dump(sDump);
        ALOGE("%s",sDump.string());
    }

    return 0;
}

int MDPComp::configureBaseLayer(hwc_context_t *ctx, ovutils::eDest dest) {
	overlay::Overlay& ov = *(ctx->mOverlay);

	ovutils::Whf info(
			ctx->dpyAttr[mDpy].xres, ctx->dpyAttr[mDpy].yres,
			MDP_RGB_888, 
			ctx->dpyAttr[mDpy].stride * ctx->dpyAttr[mDpy].yres);
	ovutils::PipeArgs parg(
			ovutils::OV_MDP_BLEND_FG_PREMULT,
            info,
            ovutils::ZORDER_0,
            ovutils::IS_FG_SET,
            ovutils::ROT_FLAGS_NONE,
            ovutils::DEFAULT_PLANE_ALPHA,
            ovutils::OVERLAY_BLENDING_OPAQUE);
	ov.setSource(parg, dest);

	ovutils::Dim rect(0, 0,
    	ctx->dpyAttr[mDpy].xres,
        ctx->dpyAttr[mDpy].yres);
	ov.setCrop(rect, dest);
    ov.setPosition(rect, dest);

	ov.setTransform(ovutils::OVERLAY_TRANSFORM_0, dest);

	if (!ov.commit(dest)) {
		ALOGE("%s: commit fails", __FUNCTION__);
        return -1;
	}
	
	return 0;
}

/*
 * Configures pipe(s) for MDP composition
 */
int MDPComp::configure(hwc_context_t *ctx, hwc_layer_1_t *layer,
                             PipeLayerPair& PipeLayerPair) {
    MdpPipeInfo& mdp_info = *PipeLayerPair.pipeInfo;
    eMdpFlags mdpFlags = ovutils::OV_MDP_BACKEND_COMPOSITION;
    eZorder zOrder = static_cast<eZorder>(mdp_info.zOrder);
    eIsFg isFg = (zOrder == ovutils::ZORDER_0)?IS_FG_SET:IS_FG_OFF;
    eDest dest = mdp_info.index;

    ALOGD_IF(isDebug(),"%s: configuring: layer: %p z_order: %d dest_pipe: %d",
             __FUNCTION__, layer, zOrder, dest);

    return configureLowRes(ctx, layer, mDpy, mdpFlags, zOrder, isFg, dest,
                           &PipeLayerPair.rot);
}

int MDPComp::pipesNeeded(hwc_context_t *ctx,
                               hwc_display_contents_1_t* list) {
    return mCurrentFrame.mdpCount;
}

bool MDPComp::allocLayerPipes(hwc_context_t *ctx,
                                    hwc_display_contents_1_t* list) {
    if (!mCurrentFrame.isFBComposed[0] && 
    	!isValidBaseLayer(ctx, &list->hwLayers[0])) {
    	mCurrentFrame.mdpBasePipe = getMdpPipe(ctx, MDPCOMP_OV_ANY);
		if (mCurrentFrame.mdpBasePipe == ovutils::OV_INVALID) {
			ALOGD_IF(isDebug(), "%s: Unable to get pipe for base pipe",
                         __FUNCTION__);
            return false;
        }
    }

    if(isYuvPresent(ctx, mDpy)) {
        int nYuvCount = ctx->listStats[mDpy].yuvCount;

        for(int index = 0; index < nYuvCount ; index ++) {
            int nYuvIndex = ctx->listStats[mDpy].yuvIndices[index];

            if(mCurrentFrame.isFBComposed[nYuvIndex])
                continue;

            hwc_layer_1_t* layer = &list->hwLayers[nYuvIndex];

            int mdpIndex = mCurrentFrame.layerToMDP[nYuvIndex];

            PipeLayerPair& info = mCurrentFrame.mdpToLayer[mdpIndex];
            info.pipeInfo = new MdpPipeInfo;
            info.rot = NULL;
            MdpPipeInfo& pipe_info = *info.pipeInfo;

            pipe_info.index = getMdpPipe(ctx, MDPCOMP_OV_VG);
            if(pipe_info.index == ovutils::OV_INVALID) {
                ALOGD_IF(isDebug(), "%s: Unable to get pipe for Videos",
                         __FUNCTION__);
                return false;
            }
        }
    }

    for(int index = 0 ; index < mCurrentFrame.layerCount; index++ ) {
        if(mCurrentFrame.isFBComposed[index]) continue;
        hwc_layer_1_t* layer = &list->hwLayers[index];
        private_handle_t *hnd = (private_handle_t *)layer->handle;

        if(isYuvBuffer(hnd))
            continue;

        int mdpIndex = mCurrentFrame.layerToMDP[index];

        PipeLayerPair& info = mCurrentFrame.mdpToLayer[mdpIndex];
        info.pipeInfo = new MdpPipeInfo;
        info.rot = NULL;
        MdpPipeInfo& pipe_info = *info.pipeInfo;

        ePipeType type = MDPCOMP_OV_ANY;

        if(!qhwc::needsScaling(layer) && !ctx->mNeedsRotator
           && ctx->mMDP.version >= qdutils::MDSS_V5) {
            type = MDPCOMP_OV_DMA;
        }

        pipe_info.index = getMdpPipe(ctx, type);
        if(pipe_info.index == ovutils::OV_INVALID) {
            ALOGD_IF(isDebug(), "%s: Unable to get pipe for UI", __FUNCTION__);
            return false;
        }
    }
    return true;
}

bool MDPComp::draw(hwc_context_t *ctx, hwc_display_contents_1_t* list) {

    if(!isEnabled()) {
        ALOGD_IF(isDebug(),"%s: MDP Comp not configured", __FUNCTION__);
        return true;
    }

    if(!ctx || !list) {
        ALOGE("%s: invalid contxt or list",__FUNCTION__);
        return false;
    }

    /* reset Invalidator */
    if(idleInvalidator && !sIdleFallBack && mCurrentFrame.mdpCount)
        idleInvalidator->markForSleep();

    overlay::Overlay& ov = *ctx->mOverlay;
    LayerProp *layerProp = ctx->layerProp[mDpy];
    
    int numHwLayers = ctx->listStats[mDpy].numAppLayers;
    if (!numHwLayers)
        return true;
    if (mCurrentFrame.mdpBasePipe != ovutils::OV_INVALID) {
    	if (!ov.queueBuffer(-1, 0, mCurrentFrame.mdpBasePipe)) {
            ALOGE("%s: queueBuffer failed for external", __FUNCTION__);
            return false;
        }
    }
    for(int i = 0; i < numHwLayers && mCurrentFrame.mdpCount; i++ )
    {
        if(mCurrentFrame.isFBComposed[i]) continue;

        hwc_layer_1_t *layer = &list->hwLayers[i];
        private_handle_t *hnd = (private_handle_t *)layer->handle;
        if(!hnd) {
            ALOGE("%s handle null", __FUNCTION__);
            return false;
        }

        int mdpIndex = mCurrentFrame.layerToMDP[i];

        MdpPipeInfo& pipe_info = *mCurrentFrame.mdpToLayer[mdpIndex].pipeInfo;
        ovutils::eDest dest = pipe_info.index;
        if(dest == ovutils::OV_INVALID) {
            ALOGE("%s: Invalid pipe index (%d)", __FUNCTION__, dest);
            return false;
        }

        if(!(layerProp[i].mFlags & HWC_MDPCOMP)) {
            continue;
        }

        ALOGD_IF(isDebug(),"%s: MDP Comp: Drawing layer: %p hnd: %p \
                 using  pipe: %d", __FUNCTION__, layer,
                 hnd, dest );

        int fd = hnd->fd;
        uint32_t offset = hnd->offset;
        Rotator *rot = mCurrentFrame.mdpToLayer[mdpIndex].rot;
        if(rot) {
            if(!rot->queueBuffer(fd, offset))
                return false;
            fd = rot->getDstMemId();
            offset = rot->getDstOffset();
        }

        if (!ov.queueBuffer(fd, offset, dest)) {
            ALOGE("%s: queueBuffer failed for external", __FUNCTION__);
            return false;
        }

        layerProp[i].mFlags &= ~HWC_MDPCOMP;
    }
    return true;
}

}; //namespace

