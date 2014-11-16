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

import com.android.internal.util.State;
import com.android.internal.util.StateMachine;

import com.broadcom.fm.FmReceiverAction;

import com.stericsson.hardware.fm.FmReceiver;

public class FmReceiverState extends StateMachine {
	
	private static final String TAG = "FmReceiverState";
	private static final boolean DBG = false;
	
	public static final int FM_CALLBACK = 100;
	public static final int FM_TIMEOUT = 200;
	public static final int FM_HARDWARE_READY = 300;

    private static final int START_TIMEOUT_DELAY = 5000;
    private static final int RESET_TIMEOUT_DELAY = 5000;
    private static final int PROPERTY_OP_DELAY =2000;

    private InvalidState mInvalidState = new InvalidState();
    private IdleState mIdleState = new IdleState();
    private StartedState mStartedState = new StartedState();
    private PausedState mPausedState = new PausedState();
    private ScaningState mScaningState = new ScaningState();
    private ErrorState mErrorState = new ErrorState();
    private PendingState mPendingState = new PendingState();

    private FmReceiverHandler mHandler;
    private int mState = -1;
	
	public FmReceiverState(FmReceiverHandler handler) {
        super("FmReceiverState:");
        addState(mInvalidState);
        addState(mIdleState);
        addState(mStartedState);
        addState(mPausedState);
        addState(mScaningState);
        addState(mErrorState);
        addState(mPendingState);
        setInitialState(mInvalidState);
        mHandler = handler;
        mState = FmReceiver.STATE_IDLE;
    }
	
	public int getState() {
		return mState;
	}
	
	private void setState(int state) {
		if (mState != state) {
			int oldState = mState;
			mState = state;
			mHandler.notifyOnStateChanged(oldState, state);
			if (state == FmReceiver.STATE_IDLE)
				mHandler.notifyOnForcedReset(0);
			if (oldState == FmReceiver.STATE_STARTING && 
				state == FmReceiver.STATE_STARTED)
				mHandler.notifyOnStarted();
		}
	}

	private boolean processAction(Message msg, long timeout, 
			State srcState, State dstState) {
		FmReceiverAction action = mHandler.processAction(msg);
		if (action == null)
 		   return false;
		mPendingState.setAction(action, srcState, dstState);
		transitionTo(mPendingState);
		if (timeout > 0)
			sendMessageDelayed(FM_TIMEOUT, timeout);
		return true;
	}

    private class InvalidState extends State {
        @Override
        public void enter() {
        	if (DBG) Log.d(TAG, "Entering InvalidState");
        	setState(-1);
        }

        @Override
        public void exit() {
        	if (DBG) Log.d(TAG, "Exit InvalidState");
        }

        @Override
        public boolean processMessage(Message msg) {
            if (msg.what == FM_HARDWARE_READY) {
               transitionTo(mIdleState);
            } else {
               deferMessage(msg);
            }
            return true;
        }
    }

    private class IdleState extends State {
        @Override
        public void enter() {
        	if (DBG) Log.d(TAG, "Entering IdleState");
        	setState(FmReceiver.STATE_IDLE);
        }

        @Override
        public void exit() {
        	if (DBG) Log.d(TAG, "Exit IdleState");
        	setState(FmReceiver.STATE_STARTING);
        }

        @Override
        public boolean processMessage(Message msg) {
        	if (DBG)
        		Log.d(TAG, "CURRENT_STATE=IDLE, MESSAGE=" + 
        				FmReceiverAction.actionToString(msg.what));
            switch(msg.what) {
               case FmReceiverAction.FM_START:
            	   processAction(msg, START_TIMEOUT_DELAY, mIdleState, mStartedState);
            	   break;
               case FmReceiverAction.FM_RESET:
               case FmReceiverAction.FM_STOP_SCAN:
                   break;
               default:
                   return false;
            }
            return true;
        }
    }

    private class StartedState extends State {
        @Override
        public void enter() {
        	if (DBG) Log.d(TAG, "Entering StartedState");
        	setState(FmReceiver.STATE_STARTED);
        }

        @Override
        public boolean processMessage(Message msg) {
        	if (DBG)
        		Log.d(TAG, "CURRENT_STATE=STARTED, MESSAGE=" + 
        				FmReceiverAction.actionToString(msg.what));
            switch(msg.what) {
               case FmReceiverAction.FM_RESET:
            	   processAction(msg, RESET_TIMEOUT_DELAY, mStartedState, mIdleState);
            	   break;
               case FmReceiverAction.FM_PAUSE:
            	   processAction(msg, PROPERTY_OP_DELAY, mStartedState, mPausedState);
            	   break;
               case FmReceiverAction.FM_SCAN_UP:
               case FmReceiverAction.FM_SCAN_DOWN:
               case FmReceiverAction.FM_FULL_SCAN:
            	   mScaningState.SetScanMode(msg.what);
            	   processAction(msg, PROPERTY_OP_DELAY, mStartedState, mScaningState);
            	   break;
               case FmReceiverAction.FM_SET_FREQUENCY:
               case FmReceiverAction.FM_SET_THRESHOLD:
               case FmReceiverAction.FM_SET_FORCE_MONO:
               case FmReceiverAction.FM_SET_AUTO_AF_SWITCH:
               case FmReceiverAction.FM_SET_AUTO_TA_SWITCH:
            	   processAction(msg, PROPERTY_OP_DELAY, mStartedState, mStartedState);
            	   break;
               case FmReceiverAction.FM_RESUME:
               case FmReceiverAction.FM_STOP_SCAN:
                    break;
               default:
                   return false;
            }
            return true;
        }
    }

    private class PausedState extends State {
        @Override
        public void enter() {
        	if (DBG) Log.d(TAG, "Entering PausedState");
        	setState(FmReceiver.STATE_PAUSED);
        }

        @Override
        public boolean processMessage(Message msg) {
        	if (DBG)
        		Log.d(TAG, "CURRENT_STATE=PAUSED, MESSAGE=" + 
        				FmReceiverAction.actionToString(msg.what));
            switch(msg.what) {
               case FmReceiverAction.FM_RESET:
            	   processAction(msg, RESET_TIMEOUT_DELAY, mPausedState, mIdleState);
            	   break;
               case FmReceiverAction.FM_RESUME:
            	   processAction(msg, PROPERTY_OP_DELAY, mPausedState, mStartedState);
            	   break;
               case FmReceiverAction.FM_SCAN_UP:
               case FmReceiverAction.FM_SCAN_DOWN:
               case FmReceiverAction.FM_FULL_SCAN:
            	   mScaningState.SetScanMode(msg.what);
            	   processAction(msg, PROPERTY_OP_DELAY, mPausedState, mScaningState);
            	   break;
               case FmReceiverAction.FM_SET_FREQUENCY:
               case FmReceiverAction.FM_SET_THRESHOLD:
               case FmReceiverAction.FM_SET_FORCE_MONO:
               case FmReceiverAction.FM_SET_AUTO_AF_SWITCH:
               case FmReceiverAction.FM_SET_AUTO_TA_SWITCH:
            	   processAction(msg, PROPERTY_OP_DELAY, mPausedState, mPausedState);
            	   break;
               case FmReceiverAction.FM_PAUSE:
               case FmReceiverAction.FM_STOP_SCAN:
            	   break;
               default:
                   if (DBG) Log.d(TAG, "ERROR: UNEXPECTED MESSAGE: CURRENT_STATE=PAUSED, MESSAGE = " + msg.what);
                   return false;
            }
            return true;
        }
    }

    private class ScaningState extends State {
    	private int mScanMode;
    	
    	public void SetScanMode(int mode) {
    		mScanMode = mode;
    	}
    	
        @Override
        public void enter() {
        	if (DBG) Log.d(TAG, "Entering ScaningState");
        	setState(FmReceiver.STATE_SCANNING);
        	sendMessageDelayed(obtainMessage(FmReceiverAction.FM_POLL_SCAN, mScanMode), 100);
        }

        @Override
        public void exit() {
        	if (DBG) Log.d(TAG, "Exit ScaningState");
        	removeMessages(FmReceiverAction.FM_POLL_SCAN);
        }

        @Override
        public boolean processMessage(Message msg) {
        	if (DBG)
        		Log.d(TAG, "CURRENT_STATE=SCANNING, MESSAGE=" + 
        				FmReceiverAction.actionToString(msg.what));
            switch(msg.what) {
               case FmReceiverAction.FM_RESET:
            	   processAction(msg, RESET_TIMEOUT_DELAY, mScaningState, mIdleState);
                   break;
               case FmReceiverAction.FM_STOP_SCAN:
            	   msg.arg1 = mScanMode;
            	   processAction(msg, PROPERTY_OP_DELAY, mScaningState, mScaningState);
                   break;
               case FmReceiverAction.FM_SET_THRESHOLD:
               case FmReceiverAction.FM_SET_FORCE_MONO:
               case FmReceiverAction.FM_SET_AUTO_AF_SWITCH:
               case FmReceiverAction.FM_SET_AUTO_TA_SWITCH:
            	   processAction(msg, PROPERTY_OP_DELAY, mScaningState, mScaningState);
            	   break;
               case FmReceiverAction.FM_POLL_SCAN:
               case FmReceiverAction.FM_SCAN_STEP:
            	   processAction(msg, PROPERTY_OP_DELAY, mScaningState, mScaningState);
            	   break;
               case FmReceiverAction.FM_SCAN_DONE:
            	   processAction(msg, PROPERTY_OP_DELAY, mScaningState, mStartedState);
            	   break;
               default:
                   return false;
            }
            return true;
        }
    }

    private class ErrorState extends State {
        @Override
        public void enter() {
        	if (DBG) Log.d(TAG, "Entering InvalidState");
        	setState(-1);
        }

        @Override
        public boolean processMessage(Message msg) {
        	if (DBG)
        		Log.d(TAG, "CURRENT_STATE=ERROR, MESSAGE=" + 
        				FmReceiverAction.actionToString(msg.what));
            return false;
        }
    }

    private class PendingState extends State {
    	private FmReceiverAction mAction;
    	private State mSrcState;
    	private State mDstState;
    	
    	void setAction(FmReceiverAction action, State srcState, State dstState) {
    		mAction = action;
    		mSrcState = srcState;
    		mDstState = dstState;
    	}
    	
        @Override
        public void enter() {
        	if (DBG) Log.d(TAG, "Entering PendingState");
        	mAction.execute();
        	int state = mAction.getState();
        	if (state == FmReceiverAction.ACTION_DONE)
            	transitionTo(mDstState);
        	else if (state == FmReceiverAction.ACTION_CANCEL)
        		transitionTo(mSrcState);
        	else if (state == FmReceiverAction.ACTION_FAIL)
        		transitionTo(mSrcState);
        	else if (state == FmReceiverAction.ACTION_ERROR)
        		transitionTo(mErrorState);
        }

        @Override
        public void exit() {
        	if (DBG) Log.d(TAG, "Exit PendingState");
        	removeMessages(FM_TIMEOUT);
        }

        @Override
        public boolean processMessage(Message msg) {
        	if (DBG)
        		Log.d(TAG, "CURRENT_STATE=PENDING, MESSAGE=" + 
        				FmReceiverAction.actionToString(msg.what));
            switch(msg.what) {
            case FM_CALLBACK:
            	mAction.callback(msg);
            	int state = mAction.getState();
            	if (state == FmReceiverAction.ACTION_DONE)
                	transitionTo(mDstState);
            	else if (state == FmReceiverAction.ACTION_CANCEL)
            		transitionTo(mSrcState);
            	else if (state == FmReceiverAction.ACTION_FAIL)
            		transitionTo(mSrcState);
            	else if (state == FmReceiverAction.ACTION_ERROR)
            		transitionTo(mErrorState);
            	break;
            case FM_TIMEOUT:
            	transitionTo(mErrorState);
            	break;
            default:
                deferMessage(msg);
                break;
            }
            return true;
        }
    }
}
