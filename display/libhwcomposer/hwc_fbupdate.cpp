/*
 * Copyright (C) 2010 The Android Open Source Project
 * Copyright (C) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Not a Contribution, Apache license notifications and license are
 * retained for attribution purposes only.
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

#define DEBUG_FBUPDATE 0
#include <gralloc_priv.h>
#include "hwc_fbupdate.h"

namespace qhwc {

namespace ovutils = overlay::utils;

IFBUpdate* IFBUpdate::getObject(const int& width, const int& dpy) {
    return new FBUpdateLowRes(dpy);
}

inline void IFBUpdate::reset() {
    mModeOn = false;
}

//================= Low res====================================
FBUpdateLowRes::FBUpdateLowRes(const int& dpy): IFBUpdate(dpy) {}

inline void FBUpdateLowRes::reset() {
    IFBUpdate::reset();
    mDest = ovutils::OV_INVALID;
}

bool FBUpdateLowRes::prepare(hwc_context_t *ctx, hwc_display_contents_1 *list,
                             int fbZorder) {
    if(!ctx->mMDP.hasOverlay) {
        ALOGD_IF(DEBUG_FBUPDATE, "%s, this hw doesnt support overlays",
                 __FUNCTION__);
        return false;
    }
    mZorder = fbZorder;
    mModeOn = configure(ctx, list, fbZorder);
    return mModeOn;
}

// Configure
bool FBUpdateLowRes::configure(hwc_context_t *ctx, hwc_display_contents_1 *list,
                               int fbZorder) {
    bool ret = false;
    hwc_layer_1_t *layer = &list->hwLayers[list->numHwLayers - 1];
    if (LIKELY(ctx->mOverlay)) {
        overlay::Overlay& ov = *(ctx->mOverlay);
        private_handle_t *hnd = (private_handle_t *)layer->handle;
        ovutils::Whf info(getWidth(hnd), getHeight(hnd),
                          ovutils::getMdpFormat(hnd->format), hnd->size);

        //Request an RGB pipe
        ovutils::eDest dest = ov.nextPipe(ovutils::OV_MDP_PIPE_ANY, mDpy);
        if(dest == ovutils::OV_INVALID) { //None available
            ALOGE("%s: No pipes available to configure framebuffer",
                __FUNCTION__);
            return false;
        }

        mDest = dest;

        ovutils::eMdpFlags mdpFlags = ovutils::OV_MDP_BLEND_FG_PREMULT;

        ovutils::eZorder zOrder = static_cast<ovutils::eZorder>(fbZorder);

        //XXX: FB layer plane alpha is currently sent as zero from
        //surfaceflinger
        ovutils::PipeArgs parg(mdpFlags,
                info,
                zOrder,
                ovutils::IS_FG_OFF,
                ovutils::ROT_FLAGS_NONE,
                ovutils::DEFAULT_PLANE_ALPHA,
                (ovutils::eBlending) getBlending(layer->blending));
        ov.setSource(parg, dest);

        hwc_rect_t sourceCrop;
		if (fbZorder == 0)
			sourceCrop = layer->sourceCrop;
		else
        	getNonWormholeRegion(list, sourceCrop);
        // x,y,w,h
        ovutils::Dim dcrop(sourceCrop.left, sourceCrop.top,
                           sourceCrop.right - sourceCrop.left,
                           sourceCrop.bottom - sourceCrop.top);
        ov.setCrop(dcrop, dest);

        int transform = layer->transform;
        ovutils::eTransform orient =
            static_cast<ovutils::eTransform>(transform);
        ov.setTransform(orient, dest);

        hwc_rect_t displayFrame = sourceCrop;
        ovutils::Dim dpos(displayFrame.left,
                          displayFrame.top,
                          displayFrame.right - displayFrame.left,
                          displayFrame.bottom - displayFrame.top);
        // Calculate the actionsafe dimensions for External(dpy = 1 or 2)
        if(mDpy)
            getActionSafePosition(ctx, mDpy, dpos.x, dpos.y, dpos.w, dpos.h);
        ov.setPosition(dpos, dest);

        ret = true;
        if (!ov.commit(dest)) {
            ALOGE("%s: commit fails", __FUNCTION__);
            ret = false;
        }
    }
    return ret;
}

bool FBUpdateLowRes::draw(hwc_context_t *ctx, private_handle_t *hnd)
{
    if(!mModeOn) {
        return true;
    }
    bool ret = true;
    overlay::Overlay& ov = *(ctx->mOverlay);
    ovutils::eDest dest = mDest;
    if (!ov.queueBuffer(hnd->fd, hnd->offset, dest)) {
        ALOGE("%s: queueBuffer failed for FBUpdate", __FUNCTION__);
        ret = false;
    }
    return ret;
}

//---------------------------------------------------------------------
}; //namespace qhwc
