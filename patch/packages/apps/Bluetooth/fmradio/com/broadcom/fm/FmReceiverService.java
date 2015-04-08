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

import android.app.Service;
import android.bluetooth.IBluetooth;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.util.Log;

import com.android.bluetooth.btservice.AdapterService;

import com.stericsson.hardware.fm.IFmReceiver;

/**
 * The implementation of the FM receiver service.
 *
 * @hide
 */
public class FmReceiverService extends Service implements ServiceConnection {

    private static final String TAG = "FmReceiverService";

	private FmReceiverHandler mBinder;
	private IBluetooth mBluetoothService;

	public IBinder onBind(Intent paramIntent) {
		Log.d(TAG, "Binding service...");
		return mBinder;
	}
  
	public void onCreate() {
		super.onCreate();
    	Log.v(TAG, "onCreate");
    	if (!bindService(new Intent(this, AdapterService.class),
                this, Context.BIND_AUTO_CREATE)) {
    		Log.e(TAG, "Could not bind to Bluetooth Service");
    		return;
        }
    	mBinder = new FmReceiverHandler(this);
    	mBinder.init();
	}
  
	public void onDestroy() {
		super.onDestroy();
		Log.d(TAG, "onDestroy");
		mBinder.cleanup();
		mBinder = null;
		unbindService(this);
		Log.d(TAG, "onDestroy done");
	}
  
	public int onStartCommand(Intent intent, int flags, int startId) {
		return START_NOT_STICKY;
	}

	public void onServiceConnected(ComponentName name, IBinder service) {
		Log.d(TAG, "onServiceConnected() name = " + name);
		mBluetoothService = (IBluetooth) IBluetooth.Stub.asInterface(service);
		mBinder.onHardwareReady();
	}

	public void onServiceDisconnected(ComponentName name) {
		Log.d(TAG, "onServiceDisconnected()");
		mBluetoothService = null;
	}
}
