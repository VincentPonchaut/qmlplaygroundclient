package com.mycompany.qmlplaygroundclient;

import org.qtproject.qt5.android.bindings.QtApplication;
import org.qtproject.qt5.android.bindings.QtActivity;

import android.content.Context;
import android.net.wifi.WifiManager;
import android.util.Log;

public final class MulticastLockJniBridgeUtil
{
	private static final String LOG_TAG = "qmlplaygroundclient";

	private static WifiManager.MulticastLock cLock;

	private MulticastLockJniBridgeUtil()
	{
	}


	public static synchronized void acquire(Context pContext)
	{
		if (cLock == null)
		{
			WifiManager wifi = (WifiManager) pContext.getSystemService(Context.WIFI_SERVICE);
			cLock = wifi.createMulticastLock("qmlplaygroundclient");
			cLock.setReferenceCounted(true);
		}

		cLock.acquire();
		Log.d(LOG_TAG, "Multicast lock: " + cLock.toString());
	}


	public static synchronized void release(Context pContext)
	{
		cLock.release();
		Log.d(LOG_TAG, "Multicast lock released.");
	}


}