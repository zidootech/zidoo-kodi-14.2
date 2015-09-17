package org.xbmc.kodi;
import android.util.AttributeSet;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceHolder.Callback;
import android.view.SurfaceView;
import android.content.Context;

import org.xbmc.kodi.Main;



public class VideoPlayView extends SurfaceView implements SurfaceHolder.Callback
{
	private static final String TAG = "VideoPlayView";
  Main mSurfaceLister = null;
  SurfaceHolder mSurfaceHolder = null;
  
	public VideoPlayView(Context context) {
			super(context);
			getHolder().addCallback(this);
			Log.d(TAG,"public VideoPlayView(Context context)");
	}

	public VideoPlayView(Context context, AttributeSet attrs) {
		this(context, attrs, 0);
		getHolder().addCallback(this);
		Log.d(TAG,"public VideoPlayView(Context context, AttributeSet attrs)");
	}

	public VideoPlayView(Context context, AttributeSet attrs, int defStyle) {
		super(context, attrs, defStyle);
		getHolder().addCallback(this);
		Log.d(TAG,"public VideoPlayView(Context context, AttributeSet attrs, int defStyle)");
	}
  
	public Surface getSurface()
	{
		Log.d(TAG,"getSurface() = " + getHolder());
		if(getHolder() == null)
		{
			return null;
		}
		else
		{
			return getHolder().getSurface();
		}
	}

	@Override
	public void surfaceCreated(SurfaceHolder holder) {
		// TODO Auto-generated method stub
		Log.d(TAG,"VideoPlayerView Ctreated");
    mSurfaceHolder = holder;
    if (mSurfaceLister != null) {
      mSurfaceLister._SetVideoPlaySurface(holder.getSurface());
    }
	}

	@Override
	public void surfaceChanged(SurfaceHolder holder, int format, int width,
			int height) {
		// TODO Auto-generated method stub
		Log.d(TAG,"VideoPlayerView Changed, format:" + format + ", width:" + width + ", height:" + height);
	}

	@Override
	public void surfaceDestroyed(SurfaceHolder holder) {
		// TODO Auto-generated method stub
		Log.d(TAG,"VideoPlayerView Destroyed");
    mSurfaceHolder = null;
    if (mSurfaceLister != null) {
      mSurfaceLister._SetVideoPlaySurface(null);
    }
	}
  
  public void setSurfaceListener(Main listener) {
    mSurfaceLister = listener;
  }
}

