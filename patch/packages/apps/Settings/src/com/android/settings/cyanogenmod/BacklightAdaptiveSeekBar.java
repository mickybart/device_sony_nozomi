/*
 * Copyright (C) 2013 The CyanogenMod Project
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

package com.android.settings.cyanogenmod;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.SeekBar;

public class BacklightAdaptiveSeekBar extends SeekBar {
    private int mMax;
    private int mGap;
    private boolean mUpdatingThumb;

    public BacklightAdaptiveSeekBar(Context context) {
        super(context);
    }

    public BacklightAdaptiveSeekBar(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public BacklightAdaptiveSeekBar(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        mUpdatingThumb = true;
        super.onSizeChanged(w, h, oldw, oldh);
        mUpdatingThumb = false;
    }

    @Override
    public void setThumb(Drawable thumb) {
        mUpdatingThumb = true;
        super.setThumb(thumb);
        mUpdatingThumb = false;
    }

    @Override
    public void setMax(int max) {
        mMax = max;
        mGap = max / 10;
        super.setMax(max + 2 * mGap);
    }

    @Override
    public void setProgress(int progress) {
        if (progress > mMax) {
            progress = getMax();
        }
        super.setProgress(progress);
    }

    @Override
    public int getProgress() {
        int progress = super.getProgress();
        if (mUpdatingThumb) {
            return progress;
        } else if (progress == getMax()) {
            return mMax + 1;
        }
        return progress;
    }

    @Override
    protected int updateTouchProgress(int lastProgress, int newProgress) {
        if (newProgress < mMax) {
            return newProgress;
        }
        if (newProgress < mMax + mGap) {
            return mMax;
        }
        return getMax();
    }
}
