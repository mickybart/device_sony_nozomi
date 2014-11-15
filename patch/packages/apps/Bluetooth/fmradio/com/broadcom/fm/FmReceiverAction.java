/*
 * Copyright (C) 2012 The Android Open Source Project
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

package com.broadcom.fm;

import android.os.Message;
import android.util.Log;

public abstract class FmReceiverAction {
	private static final String TAG = "FmReceiverAction";
	private static final boolean DBG = false;

	public static final int FM_START = 0;
	public static final int FM_RESET = 1;
	public static final int FM_PAUSE = 2;
	public static final int FM_RESUME = 3;
	public static final int FM_SET_FREQUENCY = 4;
	public static final int FM_SCAN_UP = 5;
	public static final int FM_SCAN_DOWN = 6;
	public static final int FM_FULL_SCAN = 7;
	public static final int FM_STOP_SCAN = 8;
	public static final int FM_SET_THRESHOLD = 9;
	public static final int FM_SET_FORCE_MONO = 10;
	public static final int FM_SET_AUTO_AF_SWITCH = 11;
	public static final int FM_SET_AUTO_TA_SWITCH = 12;
	public static final int FM_SEND_EXTRA_COMMAND = 13;
	
	public static final int FM_GET_RSSI = 20;
	public static final int FM_GET_FLAG = 21;
	public static final int FM_POLL_SCAN = 22;
	public static final int FM_SCAN_STEP = 23;
	public static final int FM_SCAN_DONE = 24;

	public static final int ACTION_IDLE = 0;
	public static final int ACTION_RUNNING = 1;
	public static final int ACTION_DONE = 2;
	public static final int ACTION_CANCEL = 3;
	public static final int ACTION_FAIL = 4;
	public static final int ACTION_ERROR = 5;
	
	private int mState = ACTION_IDLE;
	private Message mMsg;
	
	public FmReceiverAction(Message msg) {
		mMsg = msg;
	}
	
	public int getAction() {
		return mMsg.what;
	}
	
	public int getState() {
		return mState;
	}

	protected void setState(int state) {
		if (DBG) Log.d(TAG, " Action " + getAction() + " state change to " + state);
		mState = state;
		if (state >= ACTION_DONE) {
			synchronized(mMsg) {
				mMsg.notifyAll();
			}
			if (state == ACTION_DONE) {
				onDone();
			} else if (state == ACTION_CANCEL) {
				onCancel();
			} else if (state == ACTION_FAIL) {
				onFail();
			} else {
				onError();
			}
		}
	}

	protected void onDone() {
		
	}

	protected void onCancel() {
		
	}

	protected void onFail() {
		
	}

	protected void onError() {
		
	}

	public void execute() {
		mState = ACTION_RUNNING;
	}
	
	public void cancel() {
		mState = ACTION_CANCEL;
	}

	public abstract void callback(Message msg);

	public static String actionToString(int action) {
		switch (action) {
		default: 
			return "UNKNOWN Action: " + action;
		case FM_START: 
			return "FM_START";
		case FM_RESET: 
			return "FM_RESET";
		case FM_PAUSE: 
			return "FM_PAUSE";
		case FM_RESUME: 
			return "FM_RESUME";
		case FM_SET_FREQUENCY: 
			return "FM_SET_FREQUENCY";
		case FM_SCAN_UP: 
			return "FM_SCAN_UP";
		case FM_SCAN_DOWN: 
			return "FM_SCAN_DOWN";
		case FM_FULL_SCAN: 
			return "FM_FULL_SCAN";
		case FM_STOP_SCAN: 
			return "FM_STOP_SCAN";
		case FM_SET_THRESHOLD: 
			return "FM_SET_THRESHOLD";
		case FM_SET_FORCE_MONO: 
			return "FM_SET_FORCE_MONO";
		case FM_SET_AUTO_AF_SWITCH: 
			return "FM_SET_AUTO_AF_SWITCH";
		case FM_SET_AUTO_TA_SWITCH: 
			return "FM_SET_AUTO_TA_SWITCH";
		case FM_SEND_EXTRA_COMMAND: 
			return "FM_SEND_EXTRA_COMMAND";
		}
	}
}
