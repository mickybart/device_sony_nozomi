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

import java.util.Arrays;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Message;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.provider.Settings;
import android.util.Log;

import com.broadcom.fm.FmReceiverAction;

import com.stericsson.hardware.fm.FmBand;
import com.stericsson.hardware.fm.FmReceiver;
import com.stericsson.hardware.fm.IFmReceiver;
import com.stericsson.hardware.fm.IOnAutomaticSwitchListener;
import com.stericsson.hardware.fm.IOnErrorListener;
import com.stericsson.hardware.fm.IOnExtraCommandListener;
import com.stericsson.hardware.fm.IOnForcedPauseListener;
import com.stericsson.hardware.fm.IOnForcedResetListener;
import com.stericsson.hardware.fm.IOnRDSDataFoundListener;
import com.stericsson.hardware.fm.IOnScanListener;
import com.stericsson.hardware.fm.IOnSignalStrengthListener;
import com.stericsson.hardware.fm.IOnStartedListener;
import com.stericsson.hardware.fm.IOnStateChangedListener;
import com.stericsson.hardware.fm.IOnStereoListener;

public final class FmReceiverHandler extends IFmReceiver.Stub {
	
	private static final String TAG = "FmReceiverHandler";
	private static final boolean DBG = false;
	
	private static final int MAX_STATIONS = 50;

	private Context mContext;
	private FmReceiverState mState;

	private boolean mRadioOn;
	private FmBand mBand;
	private int mFrequency;
	private int mThreshold;
	private boolean mForceMono;
	private int mSignalStrength;
	private boolean mStereo;
	private int mFlags;
	private int mStationCount;
	private int[] mStationFreq = new int[MAX_STATIONS];
	private int[] mStationStrength = new int[MAX_STATIONS];
	
    static {
        classInitNative();
    }

	public FmReceiverHandler(Context context) {
		mContext = context;
		mState = new FmReceiverState(this);
		mState.start();
	}

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent == null) {
                return;
            }

            String action = intent.getAction();
            if (action.equals(Intent.ACTION_AIRPLANE_MODE_CHANGED)) {
                Log.d(TAG, "onReceive:ACTION_AIRPLANE_MODE_CHANGED");

                if(!isAirplaneModeOn()) {
                    if (mRadioOn)
                        mState.sendMessage(mState.obtainMessage(FmReceiverAction.FM_START, mBand));
                } else {
                    mState.sendMessage(mState.obtainMessage(FmReceiverAction.FM_RESET));
                }
            }
        }
    };

    /* Returns true if airplane mode is currently on */
    private boolean isAirplaneModeOn() {
        return Settings.Global.getInt(mContext.getContentResolver(),
                Settings.Global.AIRPLANE_MODE_ON, 0) != 0;
    }

    public void init() {
    	initNative();

        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_AIRPLANE_MODE_CHANGED);
        mContext.registerReceiver(mReceiver, filter);
    }

    public void cleanup() {
    	mContext.unregisterReceiver(mReceiver);
    	reset();
    	cleanupNative();
    }

	public FmReceiverAction processAction(Message msg) {
		FmCmd[] cmds;

		switch (msg.what) {
		default:
			return new FmNativeAction(msg, null);
		case FmReceiverAction.FM_START:
			mBand = (FmBand)msg.obj;
			cmds = new FmCmd[] {
	        	new FmEnable(true),
	    	    new FmCmdWrite(FmHardware.FM_RDS_SYSTEM, 
	            	FmHardware.FM_RDS_SYSTEM_RDS | FmHardware.FM_RDS_SYSTEM_FM),
	            new FmCmdDelay(500),
	            new FmCmdWrite(FmHardware.FM_CTRL, 
	            	(mBand.getMaxFrequency() == 90000) ? FmHardware.FM_CTRL_BAND_JAPAN : FmHardware.FM_CTRL_BAND_EUROPE_US | 
	            	FmHardware.FM_CTRL_AUTO | FmHardware.FM_CTRL_STEREO),
	            new FmCmdWrite(FmHardware.FM_AUDIO_CTRL,
	            	FmHardware.FM_AUDIO_CTRL_RF_MUTE_ENABLE |
	               	FmHardware.FM_AUDIO_CTRL_ROUTE_I2S_ENABLE |
	               	FmHardware.FM_AUDIO_CTRL_DEMPH_75US),
	        };
			return new FmNativeAction(msg, cmds) {
				protected void onDone() {
					mFrequency = -1;
					mThreshold = 500;
					mForceMono = false;
					mSignalStrength = -1;
					mStereo = false;
				}
			};
		case FmReceiverAction.FM_RESET:
			cmds = new FmCmd[] {
				new FmCmdWrite(FmHardware.FM_RDS_SYSTEM, 0),
		        new FmEnable(false),
			};
			return new FmNativeAction(msg, cmds);
		case FmReceiverAction.FM_PAUSE:
			cmds = new FmCmd[] {
				new FmCmdWrite(FmHardware.FM_AUDIO_CTRL,
		        	FmHardware.FM_AUDIO_CTRL_RF_MUTE_ENABLE |
		        	FmHardware.FM_AUDIO_CTRL_MANUAL_MUTE_ON |
		           	FmHardware.FM_AUDIO_CTRL_ROUTE_I2S_ENABLE |
		           	FmHardware.FM_AUDIO_CTRL_DEMPH_75US),
			};
			return new FmNativeAction(msg, cmds);
		case FmReceiverAction.FM_RESUME:
			cmds = new FmCmd[] {
				new FmCmdWrite(FmHardware.FM_AUDIO_CTRL,
		        	FmHardware.FM_AUDIO_CTRL_RF_MUTE_ENABLE |
		        	FmHardware.FM_AUDIO_CTRL_MANUAL_MUTE_OFF |
		           	FmHardware.FM_AUDIO_CTRL_ROUTE_I2S_ENABLE |
		           	FmHardware.FM_AUDIO_CTRL_DEMPH_75US),
			};
			return new FmNativeAction(msg, cmds);
		case FmReceiverAction.FM_SET_FREQUENCY:
			final int freq = msg.arg1;
			int hwfreq = FmHardware.Freq2HwFreq(freq);
			cmds = new FmCmd[] {
	            new FmCmdWrite(FmHardware.FM_FREQ,
	            	(byte)(hwfreq & 0xff), (byte)((hwfreq >> 8) & 0xff)),	
	    	    new FmCmdWrite(FmHardware.FM_SEARCH_TUNE_MODE, 
	    	    	FmHardware.FM_PRE_SET_MODE),
			};
			return new FmNativeAction(msg, cmds) {
				protected void onDone() {
					mFrequency = freq;
				}
			};
		case FmReceiverAction.FM_SET_THRESHOLD:
			mThreshold = msg.arg1;
			return new FmNativeAction(msg, null);
		case FmReceiverAction.FM_SET_FORCE_MONO:
			final boolean forceMono = msg.arg1 != 0;
			cmds = new FmCmd[] {
				new FmCmdWrite(FmHardware.FM_CTRL, 
		           	((mBand.getMaxFrequency() == 90000) ? FmHardware.FM_CTRL_BAND_JAPAN : FmHardware.FM_CTRL_BAND_EUROPE_US) | 
		           	(forceMono ? FmHardware.FM_CTRL_MANUAL | FmHardware.FM_CTRL_MONO : FmHardware.FM_CTRL_AUTO | FmHardware.FM_CTRL_STEREO)),
			};
			return new FmNativeAction(msg, cmds) {
				protected void onDone() {
					mForceMono = forceMono;
				}
			};
		case FmReceiverAction.FM_SCAN_UP:
			cmds = new FmCmd[] {
				new FmCmdWrite(FmHardware.FM_SEARCH_CTRL, 
		           	FmHardware.FM_SEARCH_CTRL_UP | 
		           	FmHardware.FLAG_STEREO_DETECTION |
		           	FmHardware.FLAG_STEREO_ACTIVE |
		           	FmHardware.Threshold2Rssi(mThreshold)),
				new FmCmdWrite(FmHardware.FM_SEARCH_TUNE_MODE, 
				    FmHardware.FM_AUTO_SEARCH_MODE),
			};
			return new FmNativeAction(msg, cmds);
		case FmReceiverAction.FM_SCAN_DOWN:
			cmds = new FmCmd[] {
				new FmCmdWrite(FmHardware.FM_SEARCH_CTRL, 
		           	FmHardware.FM_SEARCH_CTRL_DOWN | 
		           	FmHardware.FLAG_STEREO_DETECTION |
		           	FmHardware.FLAG_STEREO_ACTIVE |
		           	FmHardware.Threshold2Rssi(mThreshold)),
				new FmCmdWrite(FmHardware.FM_SEARCH_TUNE_MODE, 
				    FmHardware.FM_AUTO_SEARCH_MODE),
			};
			return new FmNativeAction(msg, cmds);
		case FmReceiverAction.FM_FULL_SCAN:
			mStationCount = 0;
			hwfreq = FmHardware.Freq2HwFreq(mBand.getMinFrequency());
			cmds = new FmCmd[] {
		        new FmCmdWrite(FmHardware.FM_FREQ,
			        (byte)(hwfreq & 0xff), (byte)((hwfreq >> 8) & 0xff)),	
				new FmCmdWrite(FmHardware.FM_SEARCH_CTRL, 
		           	FmHardware.FM_SEARCH_CTRL_UP | 
		           	FmHardware.FLAG_STEREO_DETECTION |
		           	FmHardware.FLAG_STEREO_ACTIVE |
		           	FmHardware.Threshold2Rssi(mThreshold)),
				new FmCmdWrite(FmHardware.FM_SEARCH_TUNE_MODE, 
				    FmHardware.FM_AUTO_SEARCH_MODE),
			};
			return new FmNativeAction(msg, cmds);
		case FmReceiverAction.FM_SCAN_STEP:
			hwfreq = FmHardware.Freq2HwFreq(mFrequency + mBand.getChannelOffset());
			cmds = new FmCmd[] {
			        new FmCmdWrite(FmHardware.FM_FREQ,
					        (byte)(hwfreq & 0xff), (byte)((hwfreq >> 8) & 0xff)),	
		        new FmCmdWrite(FmHardware.FM_SEARCH_CTRL, 
		           	FmHardware.FM_SEARCH_CTRL_UP | 
		           	FmHardware.FLAG_STEREO_DETECTION |
		           	FmHardware.FLAG_STEREO_ACTIVE |
		           	FmHardware.Threshold2Rssi(mThreshold)),
				new FmCmdWrite(FmHardware.FM_SEARCH_TUNE_MODE, 
				    FmHardware.FM_AUTO_SEARCH_MODE),
			};
			return new FmNativeAction(msg, cmds);
		case FmReceiverAction.FM_POLL_SCAN:
			final int mode = msg.arg1;
			cmds = new FmCmd[] {
			    new FmCmdPollStatus(),
			    new FmCmdReadFreq(),	
			    new FmCmdReadSignal(),
			};
			return new FmNativeAction(msg, cmds) {
				protected void onDone() {
					if (mode == FmReceiverAction.FM_FULL_SCAN) {
						mStationFreq[mStationCount] = mFrequency;
						mStationStrength[mStationCount] = mSignalStrength;
						mStationCount++;
						if (mStationCount >= MAX_STATIONS) {
							mState.sendMessage(mState.obtainMessage(FmReceiverAction.FM_SCAN_DONE, mode, 0));
						} else {
							mState.sendMessage(mState.obtainMessage(FmReceiverAction.FM_SCAN_STEP));
						}
					} else {
						mState.sendMessage(mState.obtainMessage(FmReceiverAction.FM_SCAN_DONE, mode, 0));
					}
				}

				protected void onFail() {
					mState.sendMessage(mState.obtainMessage(FmReceiverAction.FM_SCAN_DONE, mode, 0));
				}
			};
		case FmReceiverAction.FM_SCAN_DONE:
			final int scanmode = msg.arg1;
			final boolean scanabort = msg.arg2 != 0;
			if (scanmode == FmReceiverAction.FM_FULL_SCAN) {
				notifyOnFullScan(
					Arrays.copyOf(mStationFreq, mStationCount),
					Arrays.copyOf(mStationStrength, mStationCount), 
					scanabort);
			} else {
				notifyOnScan(mFrequency, mSignalStrength, 
					(scanmode == FmReceiverAction.FM_SCAN_UP) ? FmReceiver.SCAN_UP : FmReceiver.SCAN_DOWN, 
					scanabort);
			}
			hwfreq = FmHardware.Freq2HwFreq(mFrequency);
			cmds = new FmCmd[] {
		    	new FmCmdWrite(FmHardware.FM_FREQ,
	            	(byte)(hwfreq & 0xff), (byte)((hwfreq >> 8) & 0xff)),	
	    	    new FmCmdWrite(FmHardware.FM_SEARCH_TUNE_MODE, 
	    	    	FmHardware.FM_PRE_SET_MODE),
			};
			return new FmNativeAction(msg, cmds);
		case FmReceiverAction.FM_STOP_SCAN:
			final int mode1 = msg.arg1;
			cmds = new FmCmd[] {
		        new FmCmdWrite(FmHardware.FM_SEARCH_TUNE_MODE, 
				    FmHardware.FM_TERMINATE_SEARCH_TUNE_MODE),
			};
			return new FmNativeAction(msg, cmds) {
				protected void onDone() {
					mState.sendMessage(mState.obtainMessage(FmReceiverAction.FM_SCAN_DONE, mode1, 1));
				}
			};
		}
	}
	
	private void debugHci(String title, int opCode, byte[] param) {
		String str = title + Integer.toHexString(opCode) + "-";
		for (int i = 0; i < param.length; i++)
			str += Integer.toHexString(param[i] & 0xff) + ",";
		Log.d(TAG, str);
	}
	
  	private void onStateChanged(int state) {
  		Message msg = mState.obtainMessage(FmReceiverState.FM_CALLBACK, 
  				-1, 0, new byte[] { (byte)state });
  		mState.sendMessage(msg);
  	}

  	private void onCommandCallback(int opcode, byte[] param) {
		if (DBG) debugHci("Callback:", opcode, param);
  		Message msg = mState.obtainMessage(FmReceiverState.FM_CALLBACK, 
  				opcode, 0, param);
  		mState.sendMessage(msg);
  	}
  	
  	public void onHardwareReady() {
  		Message msg = mState.obtainMessage(FmReceiverState.FM_HARDWARE_READY);
  		mState.sendMessage(msg);
  	}

	private abstract class FmCmd {
		public static final int SUCCESS = 0;
		public static final int CANCEL = 1;
		public static final int FAIL = 2;
		public static final int ERROR = 3;

		public abstract int execute();
		public abstract int callback(int opcode, byte[] param);
	}
	
	private class FmEnable extends FmCmd {
		private boolean mEnable;

		public FmEnable(boolean enable) {
			mEnable = enable;
		}

		public int execute() {
			boolean result;
			if (mEnable)
				result = enableNative();
			else
				result = disableNative();
			return result ? SUCCESS : ERROR;
		}
		
		public int callback(int opcode, byte[] param) {
			if (opcode != -1)
				return ERROR;
			if (param == null || (param.length != 1))
				return ERROR;
			if (mEnable) {
				if (param[0] == 0)
					return ERROR;
			} else {
				if (param[0] != 0)
					return ERROR;
			}
			return SUCCESS;
		}
	}
	
	private class FmCmdDelay extends FmCmd {
		private long mMillis;

		public FmCmdDelay(long millis) {
			mMillis = millis;
		}

		public int execute() {
			Message msg = mState.obtainMessage(FmReceiverState.FM_CALLBACK,
					-2, 0);
			mState.sendMessageDelayed(msg, mMillis);
			return SUCCESS;
		}
		
		public int callback(int opcode, byte[] param) {
			if (opcode != -2)
				return ERROR;
			return SUCCESS;
		}
	}
	
	private class FmCmdWrite extends FmCmd {
		private byte mOpcode;
		private byte[] mParam;
		private int mRegister;
		
		public FmCmdWrite(int register, int value) {
			mOpcode = 0x15;
			mParam = new byte[] { (byte)register, 0, (byte)value };
			mRegister = register;
		}

		public FmCmdWrite(int register, int valueLow, int valueHigh) {
			mOpcode = 0x15;
			mParam = new byte[] { (byte)register, 0, (byte)valueLow, (byte)valueHigh };
			mRegister = register;
		}

		public int execute() {
			if (DBG) debugHci("Command:", mOpcode, mParam);
			return sendCmdNative(mOpcode, mParam) ? SUCCESS : ERROR;
		}

		public int callback(int opcode, byte[] param) {
			if (opcode != (0xfc00 | mOpcode))
				return ERROR;
			if (param == null || (param.length != 3))
				return ERROR;
			if ((param[0] != 0) || 
				((param[1] & 0xff) != mRegister) || 
				(param[2] != 0))
				return ERROR;
			return SUCCESS;
		}
	}
  
	private class FmCmdRead extends FmCmd {
		private byte mOpcode;
		protected byte[] mParam;
		private int mRegister;
		private int mLength;
		
		public FmCmdRead(int register, int length) {
			mOpcode = 0x15;
			mParam = new byte[] { (byte)register, 1, (byte)length };
			mRegister = register;
			mLength = length;
		}

		public int execute() {
			if (DBG) debugHci("Command:", mOpcode, mParam);
			return sendCmdNative(mOpcode, mParam) ? SUCCESS : ERROR;
		}

		public int callback(int opcode, byte[] param) {
			if (opcode != (0xfc00 | mOpcode))
				return ERROR;
			if ((param == null) || (param.length != (3 + mLength)))
				return ERROR;
			if ((param[0] != 0) || 
				((param[1] & 0xff) != mRegister) || 
				(param[2] != 1))
				return ERROR;
			mParam = param;
			return SUCCESS;
		}
	}
	
	private class FmCmdReadFreq extends FmCmdRead {
		public FmCmdReadFreq() {
			super(FmHardware.FM_FREQ, 2);
		}

		public int callback(int opcode, byte[] param) {
			int result = super.callback(opcode, param);
			if (result == SUCCESS) {
				int freq = FmHardware.HwFreq2Freq(
					(mParam[3] & 0xff) | (mParam[4] & 0xff) << 8);
				int min = mBand.getMinFrequency();
				int offset = mBand.getChannelOffset();
				freq = (freq - min + offset / 2) / offset * offset + min;
				if (!mBand.isFrequencyValid(freq))
					return FAIL;
				mFrequency = freq;
			}
			return result;
		}
	}

	private class FmCmdReadSignal extends FmCmdRead {
		public FmCmdReadSignal() {
			super(FmHardware.FM_RSSI, 1);
		}

		public int callback(int opcode, byte[] param) {
			int result = super.callback(opcode, param);
			if (result == SUCCESS) {
				mSignalStrength = (mParam[3] & 0xff);
			}
			return result;
		}
	}

	private class FmCmdReadFlag extends FmCmdRead {
		public FmCmdReadFlag() {
			super(FmHardware.FM_RDS_FLAG, 2);
		}

		public int callback(int opcode, byte[] param) {
			int result = super.callback(opcode, param);
			if (result == SUCCESS) {
				mFlags = (mParam[3] & 0xff) | (mParam[4] & 0xff) << 8;
				mStereo = (mFlags & FmHardware.FLAG_STEREO_ACTIVE) != 0;
			}
			return result;
		}
	}

	private class FmCmdPollStatus extends FmCmdRead {
		public FmCmdPollStatus() {
			super(FmHardware.FM_RDS_FLAG, 2);
		}

		public int callback(int opcode, byte[] param) {
			int result = super.callback(opcode, param);
			if (result == SUCCESS) {
				int flags = (mParam[3] & 0xff) | (mParam[4] & 0xff) << 8;
				if ((flags & FmHardware.FM_FLAG_SEARCH_TUNE_FINISHED) == 0)
					return CANCEL;
				if ((flags & FmHardware.FM_FLAG_SEARCH_TUNE_FAIL) != 0) {
					return FAIL;
				}
				mStereo = (flags & FmHardware.FLAG_STEREO_ACTIVE) != 0;
			}
			return result;
		}
	}

	private class FmNativeAction extends FmReceiverAction {
		protected FmCmd[] mCmds;
		private int mIndex;
		
		public FmNativeAction(Message msg, FmCmd[] cmds) {
			super(msg);
			mCmds = cmds;
		}
		
		public void execute() {
			if (mCmds != null && mCmds.length > 0) {
				mIndex = 0;
				int result = mCmds[mIndex].execute();
				if (result == FmCmd.SUCCESS) {
					setState(ACTION_RUNNING);
				} else if (result == FmCmd.CANCEL) {
					setState(ACTION_CANCEL);
				} else if (result == FmCmd.FAIL) {
					setState(ACTION_FAIL);
				} else {
					setState(ACTION_ERROR);
				}
			} else {
				setState(ACTION_DONE);
			}
		}

		public void callback(Message msg) {
			int result = mCmds[mIndex].callback(msg.arg1, (byte[])msg.obj);
			if (result == FmCmd.SUCCESS) {
				mIndex++;
				if (mIndex >= mCmds.length)
					setState(ACTION_DONE);
				if (getState() == ACTION_RUNNING) {
					result = mCmds[mIndex].execute();
					if (result == FmCmd.CANCEL) {
						setState(ACTION_CANCEL);
					} else if (result == FmCmd.FAIL) {
						setState(ACTION_FAIL);
					} else if (result == FmCmd.ERROR) {
						setState(ACTION_ERROR);
					}
				}
			} else if (result == FmCmd.CANCEL) {
				setState(ACTION_CANCEL);
			} else if (result == FmCmd.FAIL) {
				setState(ACTION_FAIL);
			} else {
				setState(ACTION_ERROR);
			}
		}
	}
	
    public void start(FmBand band) {
        if (isAirplaneModeOn())
            return;
        mRadioOn = true;
    	Message msg = mState.obtainMessage(FmReceiverAction.FM_START, band);
    	mState.sendMessage(msg);
		synchronized (msg) {
    		try {
				msg.wait(5000);
			} catch (InterruptedException e) {
			
			}
		}
    }

    public void startAsync(FmBand band) {
        if (isAirplaneModeOn())
            return;
        mRadioOn = true;
    	Message msg = mState.obtainMessage(FmReceiverAction.FM_START, band);
    	mState.sendMessage(msg);
    }

    public void reset() {
        mRadioOn = false;
    	Message msg = mState.obtainMessage(FmReceiverAction.FM_RESET);
    	mState.sendMessage(msg);
		synchronized (msg) {
    		try {
				msg.wait(5000);
			} catch (InterruptedException e) {
			
			}
		}
    }

    public void pause() {
    	Message msg = mState.obtainMessage(FmReceiverAction.FM_PAUSE);
    	mState.sendMessage(msg);
    }

    public void resume() {
    	Message msg = mState.obtainMessage(FmReceiverAction.FM_RESUME);
    	mState.sendMessage(msg);
    }

    public int getState() {
    	synchronized (mState) {
    		return mState.getState();
    	}
    }

    public void setFrequency(int frequency) {
    	Message msg = mState.obtainMessage(FmReceiverAction.FM_SET_FREQUENCY, 
    			frequency, 0);
    	mState.sendMessage(msg);
    }

    public int getFrequency() {
    	synchronized (mState) {
    		return mFrequency;
    	}
    }

    public void scanUp() {
    	Message msg = mState.obtainMessage(FmReceiverAction.FM_SCAN_UP);
    	mState.sendMessage(msg);
    }

    public void scanDown() {
    	Message msg = mState.obtainMessage(FmReceiverAction.FM_SCAN_DOWN);
    	mState.sendMessage(msg);
    }

    public void startFullScan() {
    	Message msg = mState.obtainMessage(FmReceiverAction.FM_FULL_SCAN);
    	mState.sendMessage(msg);
    }

    public void stopScan() {
    	Message msg = mState.obtainMessage(FmReceiverAction.FM_STOP_SCAN);
    	mState.sendMessage(msg);
    }

    public boolean isRDSDataSupported() {
        return false;
    }

    public boolean isTunedToValidChannel() {
    	synchronized (mState) {
    		return mFrequency > 0;
    	}
    }

    public void setThreshold(int threshold) {
    	Message msg = mState.obtainMessage(FmReceiverAction.FM_SET_THRESHOLD, 
    			threshold, 0);
    	mState.sendMessage(msg);
    }

    public int getThreshold() {
    	synchronized (mState) {
    		return mThreshold;
    	}
    }

    public int getSignalStrength() {
    	synchronized (mState) {
    		return mSignalStrength;
    	}
    }

    public boolean isPlayingInStereo() {
    	synchronized (mState) {
    		return mStereo;
    	}
    }

    public void setForceMono(boolean forceMono) {
    	Message msg = mState.obtainMessage(FmReceiverAction.FM_SET_FORCE_MONO, 
    			forceMono ? 1 : 0, 0);
    	mState.sendMessage(msg);
    }

    public void setAutomaticAFSwitching(boolean automatic) {
    	Message msg = mState.obtainMessage(FmReceiverAction.FM_SET_AUTO_AF_SWITCH, 
    			automatic ? 1 : 0, 0);
    	mState.sendMessage(msg);
    }

    public void setAutomaticTASwitching(boolean automatic) {
    	Message msg = mState.obtainMessage(FmReceiverAction.FM_SET_AUTO_TA_SWITCH, 
    			automatic ? 1 : 0, 0);
    	mState.sendMessage(msg);
    }

    public boolean sendExtraCommand(String command, String[] extras) {
    	return false;
    }

    private RemoteCallbackList<IOnStateChangedListener> mOnStateChangedReceivers = 
    		new RemoteCallbackList<IOnStateChangedListener>();
    private RemoteCallbackList<IOnStartedListener> mOnStartedReceivers = 
    		new RemoteCallbackList<IOnStartedListener>();
    private RemoteCallbackList<IOnErrorListener> mOnErrorReceivers = 
    		new RemoteCallbackList<IOnErrorListener>();
    private RemoteCallbackList<IOnScanListener> mOnScanReceivers = 
    		new RemoteCallbackList<IOnScanListener>();
    private RemoteCallbackList<IOnForcedPauseListener> mOnForcedPauseReceivers = 
    		new RemoteCallbackList<IOnForcedPauseListener>();
    private RemoteCallbackList<IOnForcedResetListener> mOnForcedResetReceivers = 
    		new RemoteCallbackList<IOnForcedResetListener>();
    private RemoteCallbackList<IOnRDSDataFoundListener> mOnRDSDataReceivers = 
    		new RemoteCallbackList<IOnRDSDataFoundListener>();
    private RemoteCallbackList<IOnSignalStrengthListener> mOnSignalStrengthReceivers = 
    		new RemoteCallbackList<IOnSignalStrengthListener>();
    private RemoteCallbackList<IOnStereoListener> mOnStereoReceivers = 
    		new RemoteCallbackList<IOnStereoListener>();
    private RemoteCallbackList<IOnExtraCommandListener> mOnExtraCommandReceivers = 
    		new RemoteCallbackList<IOnExtraCommandListener>();
    private RemoteCallbackList<IOnAutomaticSwitchListener> mOnAutomaticSwitchReceivers = 
    		new RemoteCallbackList<IOnAutomaticSwitchListener>();

    public void addOnStateChangedListener(IOnStateChangedListener listener) {
		if (listener != null)
			mOnStateChangedReceivers.register(listener);
    }

    public void removeOnStateChangedListener(IOnStateChangedListener listener) {
		if (listener != null)
			mOnStateChangedReceivers.unregister(listener);
    }

    public void addOnStartedListener(IOnStartedListener listener) {
		if (listener != null)
			mOnStartedReceivers.register(listener);
    }

    public void removeOnStartedListener(IOnStartedListener listener) {
		if (listener != null)
			mOnStartedReceivers.unregister(listener);
    }

    public void addOnErrorListener(IOnErrorListener listener) {
		if (listener != null)
			mOnErrorReceivers.register(listener);
    }

    public void removeOnErrorListener(IOnErrorListener listener) {
		if (listener != null)
			mOnErrorReceivers.unregister(listener);
    }

    public void addOnScanListener(IOnScanListener listener) {
		if (listener != null)
			mOnScanReceivers.register(listener);
    }

    public void removeOnScanListener(IOnScanListener listener) {
		if (listener != null)
			mOnScanReceivers.unregister(listener);
    }

    public void addOnForcedPauseListener(IOnForcedPauseListener listener) {
		if (listener != null)
			mOnForcedPauseReceivers.register(listener);
    }

    public void removeOnForcedPauseListener(IOnForcedPauseListener listener) {
		if (listener != null)
			mOnForcedPauseReceivers.unregister(listener);
    }

    public void addOnForcedResetListener(IOnForcedResetListener listener) {
		if (listener != null)
			mOnForcedResetReceivers.register(listener);
    }

    public void removeOnForcedResetListener(IOnForcedResetListener listener) {
		if (listener != null)
			mOnForcedResetReceivers.unregister(listener);
    }

    public void addOnRDSDataFoundListener(IOnRDSDataFoundListener listener) {
		if (listener != null)
			mOnRDSDataReceivers.register(listener);
    }

    public void removeOnRDSDataFoundListener(IOnRDSDataFoundListener listener) {
		if (listener != null)
			mOnRDSDataReceivers.unregister(listener);
    }

    public void addOnSignalStrengthChangedListener(IOnSignalStrengthListener listener) {
		if (listener != null)
			mOnSignalStrengthReceivers.register(listener);
    }

    public void removeOnSignalStrengthChangedListener(IOnSignalStrengthListener listener) {
		if (listener != null)
			mOnSignalStrengthReceivers.unregister(listener);
    }

    public void addOnPlayingInStereoListener(IOnStereoListener listener) {
		if (listener != null)
			mOnStereoReceivers.register(listener);
    }

    public void removeOnPlayingInStereoListener(IOnStereoListener listener) {
		if (listener != null)
			mOnStereoReceivers.unregister(listener);
    }

    public void addOnExtraCommandListener(IOnExtraCommandListener listener) {
		if (listener != null)
			mOnExtraCommandReceivers.register(listener);
    }

    public void removeOnExtraCommandListener(IOnExtraCommandListener listener) {
		if (listener != null)
			mOnExtraCommandReceivers.unregister(listener);
    }

    public void addOnAutomaticSwitchListener(IOnAutomaticSwitchListener listener) {
		if (listener != null)
			mOnAutomaticSwitchReceivers.register(listener);
    }

    public void removeOnAutomaticSwitchListener(IOnAutomaticSwitchListener listener) {
		if (listener != null)
			mOnAutomaticSwitchReceivers.unregister(listener);
    }

    public void notifyOnStateChanged(int oldState, int newState) {
		if (mOnStateChangedReceivers != null) {
            int n = mOnStateChangedReceivers.beginBroadcast();
            for (int i = 0; i < n; i++) {
            	try {
            		mOnStateChangedReceivers.getBroadcastItem(i).onStateChanged(
            				oldState, newState);
            	} catch (RemoteException e) {
            		e.printStackTrace();
            	}
            }
            mOnStateChangedReceivers.finishBroadcast();
        }
    }

    public void notifyOnStarted() {
		if (mOnStartedReceivers != null) {
            int n = mOnStartedReceivers.beginBroadcast();
            for (int i = 0; i < n; i++) {
            	try {
            		mOnStartedReceivers.getBroadcastItem(i).onStarted();
            	} catch (RemoteException e) {
            		e.printStackTrace();
            	}
            }
            mOnStartedReceivers.finishBroadcast();
        }
    }

    public void notifyOnScan(int frequency, int signalLevel, int scanDirection, boolean aborted) {
		if (mOnScanReceivers != null) {
            int n = mOnScanReceivers.beginBroadcast();
            for (int i = 0; i < n; i++) {
            	try {
            		mOnScanReceivers.getBroadcastItem(i).onScan(
            				frequency, signalLevel, scanDirection, aborted);
            	} catch (RemoteException e) {
            		e.printStackTrace();
            	}
            }
            mOnScanReceivers.finishBroadcast();
        }
    }

    public void notifyOnFullScan(int[] frequency, int[] signalLevel, boolean aborted) {
		if (mOnScanReceivers != null) {
            int n = mOnScanReceivers.beginBroadcast();
            for (int i = 0; i < n; i++) {
            	try {
            		mOnScanReceivers.getBroadcastItem(i).onFullScan(
            				frequency, signalLevel, aborted);
            	} catch (RemoteException e) {
            		e.printStackTrace();
            	}
            }
            mOnScanReceivers.finishBroadcast();
        }
    }

    public void notifyOnForcedPause() {
		if (mOnForcedPauseReceivers != null) {
            int n = mOnForcedPauseReceivers.beginBroadcast();
            for (int i = 0; i < n; i++) {
            	try {
            		mOnForcedPauseReceivers.getBroadcastItem(i).onForcedPause();
            	} catch (RemoteException e) {
            		e.printStackTrace();
            	}
            }
            mOnForcedPauseReceivers.finishBroadcast();
        }
    }

    public void notifyOnForcedReset(int reason) {
		if (mOnForcedResetReceivers != null) {
            int n = mOnForcedResetReceivers.beginBroadcast();
            for (int i = 0; i < n; i++) {
            	try {
            		mOnForcedResetReceivers.getBroadcastItem(i).onForcedReset(reason);
            	} catch (RemoteException e) {
            		e.printStackTrace();
            	}
            }
            mOnForcedResetReceivers.finishBroadcast();
        }
    }

    public void notifyOnError() {
		if (mOnErrorReceivers != null) {
            int n = mOnErrorReceivers.beginBroadcast();
            for (int i = 0; i < n; i++) {
            	try {
            		mOnErrorReceivers.getBroadcastItem(i).onError();
            	} catch (RemoteException e) {
            		e.printStackTrace();
            	}
            }
            mOnErrorReceivers.finishBroadcast();
        }
    }

    public void notifyOnRDSDataFound(Bundle bundle, int frequency) {
		if (mOnRDSDataReceivers != null) {
            int n = mOnRDSDataReceivers.beginBroadcast();
            for (int i = 0; i < n; i++) {
            	try {
            		mOnRDSDataReceivers.getBroadcastItem(i).onRDSDataFound(
            				bundle, frequency);
            	} catch (RemoteException e) {
            		e.printStackTrace();
            	}
            }
            mOnRDSDataReceivers.finishBroadcast();
        }
    }

    public void notifyOnSignalStrengthChanged(int signalStrength) {
		if (mOnSignalStrengthReceivers != null) {
            int n = mOnSignalStrengthReceivers.beginBroadcast();
            for (int i = 0; i < n; i++) {
            	try {
            		mOnSignalStrengthReceivers.getBroadcastItem(i).onSignalStrengthChanged(
            				signalStrength);
            	} catch (RemoteException e) {
            		e.printStackTrace();
            	}
            }
            mOnSignalStrengthReceivers.finishBroadcast();
        }
    }

    public void notifyOnPlayingInStereo(boolean inStereo) {
		if (mOnStereoReceivers != null) {
            int n = mOnStereoReceivers.beginBroadcast();
            for (int i = 0; i < n; i++) {
            	try {
            		mOnStereoReceivers.getBroadcastItem(i).onPlayingInStereo(
            				inStereo);
            	} catch (RemoteException e) {
            		e.printStackTrace();
            	}
            }
            mOnStereoReceivers.finishBroadcast();
        }
    }

    public void notifyOnExtraCommand(String response, Bundle extras) {
		if (mOnExtraCommandReceivers != null) {
            int n = mOnExtraCommandReceivers.beginBroadcast();
            for (int i = 0; i < n; i++) {
            	try {
            		mOnExtraCommandReceivers.getBroadcastItem(i).onExtraCommand(
            				response, extras);
            	} catch (RemoteException e) {
            		e.printStackTrace();
            	}
            }
            mOnExtraCommandReceivers.finishBroadcast();
        }
    }

    public void notifyOnAutomaticSwitching(int newFrequency, int reason) {
		if (mOnAutomaticSwitchReceivers != null) {
            int n = mOnAutomaticSwitchReceivers.beginBroadcast();
            for (int i = 0; i < n; i++) {
            	try {
            		mOnAutomaticSwitchReceivers.getBroadcastItem(i).onAutomaticSwitch(
            				newFrequency, reason);
            	} catch (RemoteException e) {
            		e.printStackTrace();
            	}
            }
            mOnAutomaticSwitchReceivers.finishBroadcast();
        }
    }

	private native static void classInitNative();
    private native void initNative();
    private native void cleanupNative();
    private native boolean disableNative();
    private native boolean enableNative();
    private native boolean sendCmdNative(int opcode, byte[] param);
}
