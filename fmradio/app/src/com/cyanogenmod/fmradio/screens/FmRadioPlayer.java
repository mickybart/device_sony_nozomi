/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.cyanogenmod.fmradio.screens;

import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.os.Process;

public class FmRadioPlayer implements Runnable, AudioManager.OnAudioFocusChangeListener {

	private static final int SAMPLERATE = 48000;

    private AudioManager mAudioManager;
    private int mBufSize;
    private byte[] mAudioData;
    private Thread mThread;

    public FmRadioPlayer(Context ctx) {
    	mAudioManager = (AudioManager)ctx.getSystemService(Context.AUDIO_SERVICE);

    	mBufSize = Math.max(
           	AudioRecord.getMinBufferSize(SAMPLERATE,
           		AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_16BIT), 
           	AudioTrack.getMinBufferSize(SAMPLERATE,
           		AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT));
        mBufSize = Math.max(mBufSize, SAMPLERATE / 10 * 4);

        mAudioData = new byte[mBufSize];
    }
    
    public void start() {
    	synchronized (this) {
    		if (mThread == null) {
    			mThread = new Thread(this);
    			int result = mAudioManager.requestAudioFocus(
    					this,
                        AudioManager.STREAM_MUSIC,
                        AudioManager.AUDIOFOCUS_GAIN);
    			if (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
        			mThread.start();
    			}
    		}
    	}
    }

    public void stop() {
    	synchronized (this) {
    		if (mThread != null) {
    			mThread.interrupt();
    			try {
    				mThread.join();
    			}  catch (InterruptedException e) {
    	    		
    	    	}
    	    	mThread = null;
    			mAudioManager.abandonAudioFocus(this);
    		}
    	}
    }
    
    private void pause() {
    	synchronized (this) {
    		if (mThread != null) {
    			mThread.interrupt();
    			try {
    				mThread.join();
    			}  catch (InterruptedException e) {
    	    		
    	    	}
    		}
    	}
    }

    private void resume() {
    	synchronized (this) {
    		if (mThread != null) {
    			mThread.start();
    			try {
    				mThread.join();
    			}  catch (InterruptedException e) {
    	    		
    	    	}
    		}
    	}
    }

    protected void finalize() throws java.lang.Throwable {
    	stop();
    	super.finalize();
    }
    
    public void run() {
    	try {
    		AudioRecord recorder = new AudioRecord(
    			MediaRecorder.AudioSource.FM_TUNER, SAMPLERATE,
    			AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_16BIT, 
    			mBufSize * 4);

    		AudioTrack player = new AudioTrack(AudioManager.STREAM_MUSIC, SAMPLERATE,
    			AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT,
    			mBufSize * 4, AudioTrack.MODE_STREAM);
        
    		player.play();
    		recorder.startRecording();

    		Process.setThreadPriority(Process.THREAD_PRIORITY_AUDIO);

    		while (!Thread.currentThread().isInterrupted()) {
    			int len = recorder.read(mAudioData, 0, mAudioData.length);
    			player.write(mAudioData, 0, len);
    		}
    		
    		recorder.stop();
    		recorder.release();
    		player.stop();
    		player.release();
    	} catch (Throwable t) {
    		t.printStackTrace();
    	}
    }

	@Override
	public void onAudioFocusChange(int focusChange) {
		if (focusChange == AudioManager.AUDIOFOCUS_LOSS_TRANSIENT) {
			pause();
		} else if (focusChange == AudioManager.AUDIOFOCUS_GAIN) {
	        resume();
		} else if (focusChange == AudioManager.AUDIOFOCUS_LOSS) {
	        stop();
		}
	}
}
