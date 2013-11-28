/*
 * Copyright (C) 2010 The Android Open Source Project
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


/******************************************************************************
 **                   Edit    History                                         *
 **---------------------------------------------------------------------------*
 ** DATE          Module              DESCRIPTION                             *
 ** 22/09/2013    Hardware Composer   Responsible for processing some         *
 **                                   Hardware layers. These layers comply    *
 **                                   with display controller specification,  *
 **                                   can be displayed directly, bypass       *
 **                                   SurfaceFligner composition. It will     *
 **                                   improve system performance.             *
 ******************************************************************************
 ** File: SprdHWLayerList.cpp         DESCRIPTION                             *
 **                                   Mainly responsible for filtering HWLayer*
 **                                   list, find layers that meet OverlayPlane*
 **                                   and PrimaryPlane specifications and then*
 **                                   mark them as HWC_OVERLAY.               *
 ******************************************************************************
 ******************************************************************************
 ** Author:         zhongjun.chen@spreadtrum.com                              *
 *****************************************************************************/

#include "SprdFrameBufferHAL.h"
#include "SprdHWLayerList.h"
#include "dump.h"


using namespace android;


SprdHWLayerList::~SprdHWLayerList()
{
    if (mLayerList)
    {
        delete [] mLayerList;
        mLayerList = NULL;
    }

    if (mOSDLayerList)
    {
        delete [] mOSDLayerList;
        mOSDLayerList = NULL;
    }
    if (mVideoLayerList)
    {
        delete [] mVideoLayerList;
        mVideoLayerList = NULL;
    }
}

void SprdHWLayerList::dump_yuv(uint8_t* pBuffer,uint32_t aInBufSize)
{
    FILE *fp = fopen("/data/video.data","ab");
    fwrite(pBuffer,1,aInBufSize,fp);
    fclose(fp);
}

void SprdHWLayerList::dump_layer(hwc_layer_1_t const* l) {
    ALOGI_IF(mDebugFlag , "\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, {%d,%d,%d,%d}, {%d,%d,%d,%d}",
             l->compositionType, l->flags, l->handle, l->transform, l->blending,
             l->sourceCrop.left,
             l->sourceCrop.top,
             l->sourceCrop.right,
             l->sourceCrop.bottom,
             l->displayFrame.left,
             l->displayFrame.top,
             l->displayFrame.right,
             l->displayFrame.bottom);
}

void SprdHWLayerList:: HWCLayerPreCheck()
{
    char value[PROPERTY_VALUE_MAX];

    property_get("debug.hwc.disable", value, "0");

    if (atoi(value) == 1)
    {
        mDisableHWCFlag = true;
    }
    else
    {
        mDisableHWCFlag = false;;
    }
}

bool SprdHWLayerList::IsHWCLayer(hwc_layer_1_t *AndroidLayer)
{
    if (AndroidLayer->flags & HWC_SKIP_LAYER)
    {
        ALOGI_IF(mDebugFlag, "Skip layer");
        return false;
    }

    /*
     *  Here should check buffer usage
     * */

    return true;
}

int SprdHWLayerList:: updateGeometry(hwc_display_contents_1_t *list)
{
    mLayerCount = 0;
    mRGBLayerCount = 0;
    mYUVLayerCount = 0;
    mOSDLayerCount = 0;
    mVideoLayerCount = 0;
    mRGBLayerFullScreenFlag = false;

    if (list == NULL)
    {
        ALOGE("updateGeometry input parameter list is NULL");
        return -1;
    }
    mList = list;

    if (mLayerList)
    {
        delete [] mLayerList;
        mLayerList = NULL;
    }

    if (mOSDLayerList)
    {
       delete [] mOSDLayerList;
       mOSDLayerList = NULL;
    }

    if (mVideoLayerList)
    {
        delete [] mVideoLayerList;
        mVideoLayerList = NULL;
    }

    queryDebugFlag(&mDebugFlag);
    queryDumpFlag(&mDumpFlag);
    if (HWCOMPOSER_DUMP_ORIGINAL_LAYERS & mDumpFlag)
    {
        dumpImage(mList);
    }

    HWCLayerPreCheck();

    if (mDisableHWCFlag)
    {
        ALOGI_IF(mDebugFlag, "HWComposer is disabled now ...");
        return 0;
    }

    mLayerCount = list->numHwLayers;

    mLayerList = new SprdHWLayer[mLayerCount];
    if (mLayerList == NULL)
    {
        ALOGE("Cannot create Layer list");
        return -1;
    }

    /*
     *  mOSDLayerList and mVideoLayerList should not include
     *  FramebufferTarget layer.
     * */
    mOSDLayerList = new SprdHWLayer*[mLayerCount - 1];
    if (mOSDLayerList == NULL)
    {
        ALOGE("Cannot create OSD Layer list");
        return -1;
    }

    mVideoLayerList = new SprdHWLayer*[mLayerCount - 1];
    if (mVideoLayerList == NULL)
    {
        ALOGE("Cannot create Video Layer list");
        return -1;
    }

    mFBLayerCount = mLayerCount - 1;

    for (unsigned int i = 0; i < mLayerCount; i++)
    {
        hwc_layer_1_t *layer = &list->hwLayers[i];
        dump_layer(layer);

        mLayerList[i].setAndroidLayer(layer);

        if (!IsHWCLayer(layer))
        {
            ALOGI_IF(mDebugFlag, "NOT HWC layer");
            continue;
        }

        if (layer->compositionType == HWC_FRAMEBUFFER_TARGET)
        {
            ALOGI_IF(mDebugFlag, "HWC_FRAMEBUFFER_TARGET layer, ignore it");
            mFBTargetLayer = layer;
            continue;
        }

        //layer->compositionType = HWC_FRAMEBUFFER;
        resetOverlayFlag(&(mLayerList[i]));

        prepareOSDLayer(&(mLayerList[i]));

        prepareVideoLayer(&(mLayerList[i]));

        setOverlayFlag(&(mLayerList[i]), i);
    }

    return 0;
}

int SprdHWLayerList:: attachToDisplayPlane(SprdPrimaryPlane *mPrimary, SprdOverlayPlane *mOverlay, int *DisplayFlag)
{
    int displayType = HWC_DISPLAY_MASK;

    if (mPrimary == NULL || mOverlay == NULL || DisplayFlag == NULL)
    {
        ALOGE("attachToDisplayPlane: Input parameters is NULL");
        return -1;
    }

    revistGeometry(DisplayFlag);

    /*
     *  At present, each SprdDisplayPlane only only can handle one
     *  HWC layer.
     *  According to Android Framework definition, the smaller z-order
     *  layer is in the bottom layer list.
     *  The application layer is in the bottom layer list.
     *  Here, we forcibly attach the bottom layer to SprdDisplayPlane.
     * */
#define DEFAULT_ATTACH_LAYER 0

    bool cond = false;
#ifdef DIRECT_DISPLAY_SINGLE_OSD_LAYER
    cond = mOSDLayerCount > 0;
#else
    cond = mOSDLayerCount > 0 && mVideoLayerCount > 0;
#endif
    if (cond)
    {
        bool DirectDisplay = false;
#ifdef DIRECT_DISPLAY_SINGLE_OSD_LAYER
        DirectDisplay = ((mOSDLayerCount == 1) && (mVideoLayerCount == 0));
#endif
        /*
         *  At present, we disable the Direct Display OSD layer first
         * */
        SprdHWLayer *sprdLayer = mOSDLayerList[DEFAULT_ATTACH_LAYER];
        if (sprdLayer && sprdLayer->InitCheck())
        {
            mPrimary->AttachPrimaryLayer(sprdLayer, DirectDisplay);
            ALOGI_IF(mDebugFlag, "Attach Format:%d layer to SprdPrimaryDisplayPlane",
                     sprdLayer->getLayerFormat());

            displayType |= HWC_DISPLAY_PRIMARY_PLANE;
        }
        else
        {
            ALOGI_IF(mDebugFlag, "Attach layer to SprdPrimaryPlane failed");
            displayType &= ~HWC_DISPLAY_PRIMARY_PLANE;
        }
    }

    if (mVideoLayerCount > 0)
    {
        SprdHWLayer *sprdLayer = mVideoLayerList[DEFAULT_ATTACH_LAYER];

        if (sprdLayer && sprdLayer->InitCheck())
        {
            mOverlay->AttachOverlayLayer(sprdLayer);
            ALOGI_IF(mDebugFlag, "Attach Format:%d layer to SprdOverlayPlane",
                     sprdLayer->getLayerFormat());

            displayType |= HWC_DISPLAY_OVERLAY_PLANE;
        }
        else
        {
            ALOGI_IF(mDebugFlag, "Attach layer to SprdOverlayPlane failed");

            displayType &= ~HWC_DISPLAY_OVERLAY_PLANE;
        }
    }

    if (*DisplayFlag & HWC_DISPLAY_OVERLAY_COMPOSER_GPU)
    {
        displayType &= ~(HWC_DISPLAY_PRIMARY_PLANE | HWC_DISPLAY_OVERLAY_PLANE);
    }
    else if (mFBTargetLayer &&
             mOSDLayerCount <= 1 &&
             mVideoLayerCount <= 0)
    {
        //mPrimary->AttachFrameBufferTargetLayer(mFBTargetLayer);
        ALOGI_IF(mDebugFlag, "Attach Framebuffer Target layer");

        displayType |= (0x1) & HWC_DISPLAY_FRAMEBUFFER_TARGET;
    }
    else
    {
        displayType &= ~HWC_DISPLAY_FRAMEBUFFER_TARGET;
    }

    *DisplayFlag |= displayType;

    return 0;
}

int SprdHWLayerList:: revistGeometry(int *DisplayFlag)
{
    SprdHWLayer *YUVLayer = NULL;
    SprdHWLayer *RGBLayer = NULL;
    int YUVIndex = 0;
    int RGBIndex = 0;
    int i = -1;
    bool postProcessVideoCond = false;
    mSkipLayerFlag = false;
    int LayerCount = mLayerCount;

    if (mDisableHWCFlag)
    {
        return 0;
    }

    /*
     *  revist Overlay layer geometry.
     * */
    int VideoLayerCount = mVideoLayerCount;

    for (i = 0; i < VideoLayerCount; i++)
    {
        if (!(mVideoLayerList[i]->InitCheck()))
        {
            continue;
        }

        /*
         *  Our Display Controller cannot handle 2 or more than 2 video layers
         *  at the same time.
         * */
        if (mVideoLayerCount > 1)
        {
            resetOverlayFlag(mVideoLayerList[i]);
            mFBLayerCount++;
            continue;
        }

        YUVLayer = mVideoLayerList[i];
        YUVIndex = YUVLayer->getLayerIndex();
    }

    /*
     *  revist OSD layer geometry.
     * */
    int OSDLayerCount = mOSDLayerCount;

    for (i = 0; i < OSDLayerCount; i++)
    {
        if (!(mOSDLayerList[i]->InitCheck()))
        {
            continue;
        }

        /*
         *  At present, the HWComposer cannot handle 2 or more than 2 RGB layer.
         *  So, when found RGB layer count > 1, just switch back to SurfaceFlinger.
         * */
        if (/*mOSDLayerCount > 1 ||*/YUVLayer == NULL)
        {
            resetOverlayFlag(mOSDLayerList[i]);
            mFBLayerCount++;
            ALOGI_IF(mDebugFlag , "no video layer, abandon osd overlay");
            continue;
        }

        RGBLayer = mOSDLayerList[i];
        RGBIndex = RGBLayer->getLayerIndex();
    }

#ifdef DIRECT_DISPLAY_SINGLE_OSD_LAYER
    /*
     *  if the RGB layer is bottom layer and there is no other layer,
     *  go overlay.
     * */
    if (RGBLayer && (RGBIndex == 0) && (LayerCount == 1))
    {
        goto Overlay;
    }
#endif

    /*
     *  Make sure the OSD layer is the top layer and the layer below it
     *  is video layer. If so, go overlay.
     * */
    if (YUVLayer)
    {
        if (RGBLayer && ((RGBIndex + 1) == LayerCount) && (YUVIndex == RGBIndex -1))
        {
            goto Overlay;
        }
        else if (RGBLayer == NULL)
        {
            goto Overlay;
        }
    }

Overlay:

#ifdef OVERLAY_COMPOSER_GPU
    postProcessVideoCond = (YUVLayer /*&& (RGBLayer || FBLayerCount > 0)*/);
#else
    postProcessVideoCond = (YUVLayer && mFBLayerCount > 0);
#endif

    if (postProcessVideoCond)
    {
#ifndef OVERLAY_COMPOSER_GPU
        resetOverlayFlag(YUVLayer);
        if (RGBLayer)
        {
            resetOverlayFlag(RGBLayer);
            mFBLayerCount++;
        }
        mFBLayerCount++;
#else
        revistOverlayComposerLayer(YUVLayer, RGBLayer, LayerCount, &mFBLayerCount, DisplayFlag);
    }
#endif
    }

    ALOGI_IF(mDebugFlag, "Total layer count: %d, Framebuffer layer count: %d, OSD layer count:%d, video layer count:%d", 
            (mLayerCount - 1), mFBLayerCount, mOSDLayerCount, mVideoLayerCount);

    YUVLayer = NULL;
    RGBLayer = NULL;


    return 0;
}

void SprdHWLayerList:: ClearFrameBuffer(hwc_layer_1_t *l, unsigned int index)
{
    if (index != 0)
    {
        l->hints = HWC_HINT_CLEAR_FB;
    }
    else
    {
        l->hints &= ~HWC_HINT_CLEAR_FB;
    }
}

void SprdHWLayerList:: setOverlayFlag(SprdHWLayer *l, unsigned int index)
{
    hwc_layer_1_t *layer = l->getAndroidLayer();

    switch (l->getLayerType())
    {
        case LAYER_OSD:
            l->setSprdLayerIndex(mOSDLayerCount);
            mOSDLayerList[mOSDLayerCount] = l;
            mOSDLayerCount++;
            forceOverlay(l);
            ClearFrameBuffer(layer, index);
            break;
        case LAYER_OVERLAY:
            l->setSprdLayerIndex(mVideoLayerCount);
            mVideoLayerList[mVideoLayerCount] = l;
            mVideoLayerCount++;
            forceOverlay(l);
            ClearFrameBuffer(layer, index);
            break;
        default:
            break;
    }

    l->setLayerIndex(index);
}

void SprdHWLayerList:: forceOverlay(SprdHWLayer *l)
{
    if (l == NULL)
    {
        ALOGE("Input parameters SprdHWLayer is NULL");
        return;
    }

    hwc_layer_1_t *layer = l->getAndroidLayer();
    layer->compositionType = HWC_OVERLAY;

}

void SprdHWLayerList:: resetOverlayFlag(SprdHWLayer *l)
{
    if (l == NULL)
    {
        ALOGE("SprdHWLayer is NULL");
        return;
    }

    hwc_layer_1_t *layer = l->getAndroidLayer();
    layer->compositionType = HWC_FRAMEBUFFER;
    int index = l->getSprdLayerIndex();

    if (index < 0)
    {
        return;
    }

    switch (l->getLayerType())
    {
        case LAYER_OSD:
            mOSDLayerList[index] = NULL;
            mOSDLayerCount--;
            break;
        case LAYER_OVERLAY:
            mVideoLayerList[index] = NULL;
            mVideoLayerCount--;
            break;
        default:
            return;
    }
}

int SprdHWLayerList:: prepareOSDLayer(SprdHWLayer *l)
{
    unsigned int srcWidth;
    unsigned int srcHeight;
    hwc_layer_1_t *layer = l->getAndroidLayer();
    const native_handle_t *pNativeHandle = layer->handle;
    struct private_handle_t *privateH = (struct private_handle_t *)pNativeHandle;
    struct sprdRect *srcRect = l->getSprdSRCRect();
    struct sprdRect *FBRect  = l->getSprdFBRect();

    unsigned int mFBWidth  = mFBInfo->fb_width;
    unsigned int mFBHeight = mFBInfo->fb_height;

    if (privateH == NULL)
    {
        ALOGI_IF(mDebugFlag, "layer handle is NULL");
        return -1;
    }

    if (!(l->checkRGBLayerFormat()))
    {
        ALOGI_IF(mDebugFlag, "prepareOSDLayer NOT RGB format layer Line:%d", __LINE__);
        return 0;
    }

    mRGBLayerCount++;

    l->setLayerFormat(privateH->format);

#ifdef PROCESS_VIDEO_USE_GSP
#ifndef OVERLAY_COMPOSER_GPU
    if (!(privateH->flags & private_handle_t::PRIV_FLAGS_USES_PHY))
    {
        ALOGI_IF(mDebugFlag, "prepareOSDLayer find virtual address Line:%d", __LINE__);
        return 0;
    }
#endif
#else
    if ((layer->transform != 0) || !(privateH->flags & private_handle_t::PRIV_FLAGS_USES_PHY))
    {
        ALOGI_IF(mDebugFlag, "prepareOSDLayer L%d", __LINE__);
        return 0;
    }
#endif

    if ((layer->transform != 0) &&
        ((layer->transform & HAL_TRANSFORM_ROT_90) != HAL_TRANSFORM_ROT_90))
    {
        ALOGI_IF(mDebugFlag, "prepareOSDLayer not support the kind of rotation L%d", __LINE__);
        return 0;
    }
    else if (layer->transform == 0)
    {
        if (((unsigned int)privateH->width != mFBWidth)
            || ((unsigned int)privateH->height != mFBHeight))
        {
            ALOGI_IF(mDebugFlag, "prepareOSDLayer Not full-screen");
            return 0;
        }

#ifndef _DMA_COPY_OSD_LAYER
#ifndef OVERLAY_COMPOSER_GPU
        if (!(privateH->flags & private_handle_t::PRIV_FLAGS_USES_PHY))
        {
            ALOGI_IF(mDebugFlag, "prepareOSDLayer Not physical address %d", __LINE__);
            return 0;
        }
#endif
#endif
    }
    else if (((unsigned int)privateH->width != mFBHeight)
             || ((unsigned int)privateH->height != mFBWidth)
#ifndef OVERLAY_COMPOSER_GPU
             || !(privateH->flags & private_handle_t::PRIV_FLAGS_USES_PHY)
#endif
    )
    {
        ALOGI_IF(mDebugFlag, "prepareOSDLayer L%d", __LINE__);
        return 0;
    }

    srcRect->x = MAX(layer->sourceCrop.left, 0);
    srcRect->x = MIN(srcRect->x, privateH->width);
    srcRect->y = MAX(layer->sourceCrop.top, 0);
    srcRect->y = MIN(srcRect->y, privateH->height);
    srcRect->w = MIN(layer->sourceCrop.right - srcRect->x, privateH->width - srcRect->x);
    srcRect->h = MIN(layer->sourceCrop.bottom - srcRect->y, privateH->height - srcRect->y);

    FBRect->x = MAX(layer->displayFrame.left, 0);
    FBRect->x = MIN(FBRect->x, mFBWidth);
    FBRect->y = MAX(layer->displayFrame.top, 0);
    FBRect->y = MIN(FBRect->y, mFBHeight);
    FBRect->w = MIN(layer->displayFrame.right - FBRect->x, mFBWidth - FBRect->x);
    FBRect->h = MIN(layer->displayFrame.bottom - FBRect->y, mFBHeight - FBRect->y);

    if ((layer->transform & HAL_TRANSFORM_ROT_90) == HAL_TRANSFORM_ROT_90)
    {
        srcWidth = srcRect->h;
        srcHeight = srcRect->w;
    }
    else
    {
        srcWidth = srcRect->w;
        srcHeight = srcRect->h;
    }

    /*
     * Only support full screen now
     * */
    if ((srcWidth != FBRect->w) || (srcHeight != FBRect->h))
    {
        ALOGI_IF(mDebugFlag, "prepareOSDLayer NOT full-screen L%d", __LINE__);
        return 0;
    }

    if ((srcRect->x != 0) || (srcRect->y != 0))
    {
        ALOGI_IF(mDebugFlag, "prepareOSDLayer only support full screen now L%d", __LINE__);
        return 0;
    }

    if ((FBRect->w != mFBWidth) || (FBRect->h != mFBHeight))
    {
        ALOGI_IF(mDebugFlag, "prepareOSDLayer only support full screen now L%d", __LINE__);
        return 0;
    }

    mRGBLayerFullScreenFlag = true;

#ifdef OVERLAY_COMPOSER_GPU
    prepareOverlayComposerLayer(l);
#endif

    l->setLayerType(LAYER_OSD);

    mFBLayerCount--;

    return 0;
}

int SprdHWLayerList:: prepareVideoLayer(SprdHWLayer *l)
{
    int srcWidth;
    int srcHeight;
    int destWidth;
    int destHeight;
    hwc_layer_1_t *layer = l->getAndroidLayer();
    const native_handle_t *pNativeHandle = layer->handle;
    struct private_handle_t *privateH = (struct private_handle_t *)pNativeHandle;
    struct sprdRect *srcRect = l->getSprdSRCRect();
    struct sprdRect *FBRect  = l->getSprdFBRect();
    struct sprdYUV  *srcImg =  l->getSprdSRCYUV();

    unsigned int mFBWidth  = mFBInfo->fb_width;
    unsigned int mFBHeight = mFBInfo->fb_height;

    if (privateH == NULL)
    {
        ALOGI_IF(mDebugFlag, "layer handle is NULL");
        return -1;
    }

    if (!(l->checkYUVLayerFormat()))
    {
        ALOGI_IF(mDebugFlag, "prepareOverlayLayer L%d,color format:0x%08x,ret 0", __LINE__, privateH->format);
        return 0;
    }

    l->setLayerFormat(privateH->format);

    mYUVLayerCount++;

#if 0
    if (!(privateH->flags & private_handle_t::PRIV_FLAGS_USES_PHY)
        || (privateH->flags & private_handle_t::PRIV_FLAGS_NOT_OVERLAY)) {
        ALOGI_IF(mDebugFlag, "prepareOverlayLayer L%d,flags:0x%08x ,ret 0 \n", __LINE__, privateH->flags);
        return 0;
    }
#endif

    if(layer->blending != HWC_BLENDING_NONE)
    {
       ALOGI_IF(mDebugFlag, "prepareOverlayLayer L%d,blend:0x%08x,ret 0", __LINE__, layer->blending);
        return 0;
    }
#ifndef PROCESS_VIDEO_USE_GSP
    if(layer->transform == HAL_TRANSFORM_FLIP_V)
    {
       ALOGI_IF(mDebugFlag, "prepareOverlayLayer L%d,transform:0x%08x,ret 0", __LINE__, layer->transform);
        return 0;
    }

    if((layer->transform == (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_H)) || (layer->transform == (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_V)))
    {
        ALOGI_IF(mDebugFlag, "prepareOverlayLayer L%d,transform:0x%08x,ret 0", __LINE__, layer->transform);
        return 0;
    }
#endif
    srcImg->format = privateH->format;
    srcImg->w = privateH->width;
    srcImg->h = privateH->height;

    int rot_90_270 = (layer->transform&HAL_TRANSFORM_ROT_90) == HAL_TRANSFORM_ROT_90;

    srcRect->x = MAX(layer->sourceCrop.left, 0);
    srcRect->x = (srcRect->x + SRCRECT_X_ALLIGNED) & (~SRCRECT_X_ALLIGNED);//dcam 8 pixel crop
    srcRect->x = MIN(srcRect->x, srcImg->w);
    srcRect->y = MAX(layer->sourceCrop.top, 0);
    srcRect->y = (srcRect->y + SRCRECT_Y_ALLIGNED) & (~SRCRECT_Y_ALLIGNED);//dcam 8 pixel crop
    srcRect->y = MIN(srcRect->y, srcImg->h);

    srcRect->w = MIN(layer->sourceCrop.right - layer->sourceCrop.left, srcImg->w - srcRect->x);
    srcRect->h = MIN(layer->sourceCrop.bottom - layer->sourceCrop.top, srcImg->h - srcRect->y);

    if((srcRect->w - (srcRect->w & (~SRCRECT_WIDTH_ALLIGNED)))> ((SRCRECT_WIDTH_ALLIGNED+1)>>1))
    {
        srcRect->w = (srcRect->w + SRCRECT_WIDTH_ALLIGNED) & (~SRCRECT_WIDTH_ALLIGNED);//dcam 8 pixel crop
    } else
    {
        srcRect->w = (srcRect->w) & (~SRCRECT_WIDTH_ALLIGNED);//dcam 8 pixel crop
    }

    if((srcRect->h - (srcRect->h & (~SRCRECT_HEIGHT_ALLIGNED)))> ((SRCRECT_HEIGHT_ALLIGNED+1)>>1))
    {
        srcRect->h = (srcRect->h + SRCRECT_HEIGHT_ALLIGNED) & (~SRCRECT_HEIGHT_ALLIGNED);//dcam 8 pixel crop
    }
    else
    {
        srcRect->h = (srcRect->h) & (~SRCRECT_HEIGHT_ALLIGNED);//dcam 8 pixel crop
    }

    srcRect->w = MIN(srcRect->w, srcImg->w - srcRect->x);
    srcRect->h = MIN(srcRect->h, srcImg->h - srcRect->y);
    //--------------------------------------------------
    FBRect->x = MAX(layer->displayFrame.left, 0);
    FBRect->y = MAX(layer->displayFrame.top, 0);
    FBRect->x = MIN(FBRect->x, mFBWidth);
    FBRect->y = MIN(FBRect->y, mFBHeight);

    FBRect->w = MIN(layer->displayFrame.right - layer->displayFrame.left, mFBWidth - FBRect->x);
    FBRect->h = MIN(layer->displayFrame.bottom - layer->displayFrame.top, mFBHeight - FBRect->y);
    if((FBRect->w - (FBRect->w & (~FB_WIDTH_ALLIGNED)))> ((FB_WIDTH_ALLIGNED+1)>>1))
    {
        FBRect->w = (FBRect->w + FB_WIDTH_ALLIGNED) & (~FB_WIDTH_ALLIGNED);//dcam 8 pixel and lcdc must 4 pixel for yuv420
    }
    else
    {
        FBRect->w = (FBRect->w) & (~FB_WIDTH_ALLIGNED);//dcam 8 pixel and lcdc must 4 pixel for yuv420
    }

    if((FBRect->h - (FBRect->h & (~FB_HEIGHT_ALLIGNED)))> ((FB_HEIGHT_ALLIGNED+1)>>1))
    {
        FBRect->h = (FBRect->h + FB_HEIGHT_ALLIGNED) & (~FB_HEIGHT_ALLIGNED);//dcam 8 pixel and lcdc must 4 pixel for yuv420
    }
    else
    {
        FBRect->h = (FBRect->h) & (~FB_HEIGHT_ALLIGNED);//dcam 8 pixel and lcdc must 4 pixel for yuv420
    }


    FBRect->w = MIN(FBRect->w, mFBWidth - ((FBRect->x + FB_WIDTH_ALLIGNED) & (~FB_WIDTH_ALLIGNED)));
    FBRect->h = MIN(FBRect->h, mFBHeight - ((FBRect->y + FB_HEIGHT_ALLIGNED) & (~FB_HEIGHT_ALLIGNED)));
    ALOGV("rects {%d,%d,%d,%d}, {%d,%d,%d,%d}", srcRect->x, srcRect->y, srcRect->w, srcRect->h,
          FBRect->x, FBRect->y, FBRect->w, FBRect->h);

#ifndef PROCESS_VIDEO_USE_GSP

    if(srcRect->w < 4 || srcRect->h < 4 ||
       FBRect->w < 4 || FBRect->h < 4 ||
       FBRect->w > 960 || FBRect->h > 960)
    { //dcam scaling > 960 should use slice mode
        ALOGI_IF(mDebugFlag,"prepareOverlayLayer, dcam scaling > 960 should use slice mode! L%d",__LINE__);
        return 0;
    }
#else
    if(srcRect->w < 4
       || srcRect->h < 4
       || FBRect->w < 4
       || FBRect->h < 4
       || FBRect->w > mFBWidth // when HWC do blending by GSP, the output can't larger than LCD width and height
       || FBRect->h > mFBHeight) {
       ALOGI_IF(mDebugFlag,"prepareOverlayLayer, gsp,return 0, L%d",__LINE__);
        return 0;
    }

#endif

    destWidth = FBRect->w;
    destHeight = FBRect->h;
    if ((layer->transform&HAL_TRANSFORM_ROT_90) == HAL_TRANSFORM_ROT_90)
    {
        srcWidth = srcRect->h;
        srcHeight = srcRect->w;
    }
    else
    {
        srcWidth = srcRect->w;
        srcHeight = srcRect->h;
    }

#ifndef PROCESS_VIDEO_USE_GSP
    if(4 * srcWidth < destWidth || srcWidth > 4 * destWidth ||
       4 * srcHeight < destHeight || srcHeight > 4 * destHeight)
    { //dcam support 1/4-4 scaling
        ALOGI_IF(mDebugFlag,"prepareOverlayLayer, dcam support 1/4-4 scaling! L%d",__LINE__);
        return 0;
    }
#else
    if(4 * srcWidth < destWidth || srcWidth > 16 * destWidth ||
       4 * srcHeight < destHeight || srcHeight > 16 * destHeight)
    { //gsp support [1/16-4] scaling
        ALOGI_IF(mDebugFlag,"prepareOverlayLayer, dcam support 1/4-4 scaling! L%d",__LINE__);
        return 0;
    }

    //added for Bug 181381
    if(((srcWidth < destWidth) && (srcHeight > destHeight))
       || ((srcWidth > destWidth) && (srcHeight < destHeight)))
    { //gsp support [1/16-4] scaling
        ALOGI_IF(mDebugFlag,"prepareOverlayLayer, gsp not support one direction scaling down while the other scaling up! L%d",__LINE__);
        return 0;
    }
#endif

#ifdef OVERLAY_COMPOSER_GPU
    prepareOverlayComposerLayer(l);
#endif

    l->setLayerType(LAYER_OVERLAY);

    mFBLayerCount--;

    return 0;

}

#ifdef OVERLAY_COMPOSER_GPU
int SprdHWLayerList::prepareOverlayComposerLayer(SprdHWLayer *l)
{
    hwc_layer_1_t *layer = l->getAndroidLayer();

    if (layer == NULL)
    {
        ALOGE("prepareOverlayComposerLayer input layer is NULL");
        return -1;
    }

    if (layer->displayFrame.left < 0 ||
        layer->displayFrame.top < 0 ||
        layer->displayFrame.left > mFBInfo->fb_width ||
        layer->displayFrame.top > mFBInfo->fb_height ||
        layer->displayFrame.right > mFBInfo->fb_width ||
        layer->displayFrame.bottom > mFBInfo->fb_height)
    {
        mSkipLayerFlag = true;
    }

    return 0;
}

int SprdHWLayerList:: revistOverlayComposerLayer(SprdHWLayer *YUVLayer, SprdHWLayer *RGBLayer, int LayerCount, int *FBLayerCount, int *DisplayFlag)
{
    int displayType = HWC_DISPLAY_MASK;
    /*
     *  At present, OverlayComposer cannot handle 2 or more than 2 YUV layers.
     *  And OverlayComposer also cannot handle cropped RGB layer.
     * */
    if (mYUVLayerCount == 1)
    {
        mRGBLayerFullScreenFlag = true;
    }

    if (mYUVLayerCount > 1 || mRGBLayerFullScreenFlag == false)
    {
        ALOGI_IF(mDebugFlag, "YUVLayerCount > 1 or Find Cropped RGB layer mYUVLayerCount: %d, mRGBLayerFullScreenFlag: %d", 
                 mYUVLayerCount, mRGBLayerFullScreenFlag);
        mSkipLayerFlag = true;
    }

    if (mSkipLayerFlag == false)
    {
        for (int j = 0; j < LayerCount; j++)
        {
            SprdHWLayer *SprdLayer = &(mLayerList[j]);
            hwc_layer_1_t *l = SprdLayer->getAndroidLayer();

            if (l && (l->compositionType == HWC_FRAMEBUFFER_TARGET))
            {
                continue;
            }

            if (l->compositionType == HWC_FRAMEBUFFER)
            {
                int format = SprdLayer->getLayerFormat();
                if (SprdLayer->checkRGBLayerFormat())
                {
                    SprdLayer->setLayerType(LAYER_OSD);
                }
                else
                {
                    SprdLayer->setLayerType(LAYER_OVERLAY);
                }
                ALOGI_IF(mDebugFlag, "Force layer format:%d go into OverlayComposer", format);
                setOverlayFlag(SprdLayer, j);

                (*FBLayerCount)--;
            }
        }
        displayType |= HWC_DISPLAY_OVERLAY_COMPOSER_GPU;
    }


     /*
      *  When Skip layer is found, SurfaceFlinger maybe want to do the Animation,
      *  or other thing, here just disable OverlayComposer.
      *  Switch back to SurfaceFlinger for composition.
      *  At present, it is just a workaround method.
      * */
     if (mSkipLayerFlag)
     {
         resetOverlayFlag(YUVLayer);

         (*FBLayerCount)++;

         if (RGBLayer)
         {
             resetOverlayFlag(RGBLayer);

             (*FBLayerCount)++;
         }

         displayType &= ~HWC_DISPLAY_OVERLAY_COMPOSER_GPU;
     }

     *DisplayFlag |= displayType;

      mSkipLayerFlag = false;

      return 0;
}
#endif

int SprdHWLayerList::checkHWLayerList(hwc_display_contents_1_t* list)
{
    if (list == NULL)
    {
        ALOGE("input list is NULL");
        return -1;
    }

    if (list != mList)
    {
        ALOGE("input list has been changed");
        return -1;
    }

    if (list->numHwLayers != mLayerCount)
    {
        ALOGE("The input layer count have been changed");
        return -1;
    }

    return 0;
}
