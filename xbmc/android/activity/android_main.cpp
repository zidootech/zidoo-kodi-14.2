/*
 *      Copyright (C) 2012-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <stdlib.h>
#include <errno.h>
#include <android_native_app_glue.h>
#include "EventLoop.h"
#include "XBMCApp.h"
#include "android/jni/SurfaceTexture.h"
#include "utils/StringUtils.h"
#include "CompileInfo.h"
#include <sys/system_properties.h>

// copied from new android_native_app_glue.c
static void process_input(struct android_app* app, struct android_poll_source* source) {
    AInputEvent* event = NULL;
    int processed = 0;
    while (AInputQueue_getEvent(app->inputQueue, &event) >= 0) {
        if (AInputQueue_preDispatchEvent(app->inputQueue, event)) {
            continue;
        }
        int32_t handled = 0;
        if (app->onInputEvent != NULL) handled = app->onInputEvent(app, event);
        AInputQueue_finishEvent(app->inputQueue, event, handled);
        processed = 1;
    }
    if (processed == 0) {
        CXBMCApp::android_printf("process_input: Failure reading next input event: %s", strerror(errno));
    }
}

extern void android_main(struct android_app* state)
{
  {
    const char *support_model[] = {"ZIDOO_X1", "ZIDOO_X9"};
    char modelname[PROP_VALUE_MAX];
    int len = __system_property_get("ro.product.model", modelname);
    int support = 0;
    if (len > 0 && len <= PROP_VALUE_MAX) 
    {
        //CXBMCApp::android_printf("android_main: modelname = %s ", modelname);
        int howmany = sizeof(support_model)/sizeof(support_model[0]);
        int i = 0;
        for (i = 0; i < howmany; i++) {
            if (strcmp((const char *)modelname, (const char *)support_model[i]) == 0) {
                support = 1;
                break;
            }
        }        
    }
    
    if (support == 0) {
        //CXBMCApp::android_printf("android_main: model is %s ", (support == 1) ? "support" : "not spport!");
        CXBMCApp::android_printf("I am very sorry!! ");
        exit(0);
    }
    
    // make sure that the linker doesn't strip out our glue
    app_dummy();

    // revector inputPollSource.process so we can shut up
    // its useless verbose logging on new events (see ouya)
    // and fix the error in handling multiple input events.
    // see https://code.google.com/p/android/issues/detail?id=41755
    state->inputPollSource.process = process_input;

    CEventLoop eventLoop(state);
    CXBMCApp xbmcApp(state->activity);
    if (xbmcApp.isValid())
    {
      IInputHandler inputHandler;
      eventLoop.run(xbmcApp, inputHandler);
    }
    else
      CXBMCApp::android_printf("android_main: setup failed");

    CXBMCApp::android_printf("android_main: Exiting");
    // We need to call exit() so that all loaded libraries are properly unloaded
    // otherwise on the next start of the Activity android will simple re-use
    // those loaded libs in the state they were in when we quit XBMC last time
    // which will lead to crashes because of global/static classes that haven't
    // been properly uninitialized
  }
  exit(0);
}

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
  jint version = JNI_VERSION_1_6;
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), version) != JNI_OK)
    return -1;

  std::string appName = CCompileInfo::GetAppName();
  StringUtils::ToLower(appName);
  std::string mainClass = "org/xbmc/" + appName + "/Main";
  std::string bcReceiver = "org/xbmc/" + appName + "/XBMCBroadcastReceiver";
  std::string frameListener = "org/xbmc/" + appName + "/XBMCOnFrameAvailableListener";

  jclass cMain = env->FindClass(mainClass.c_str());
  if(cMain)
  {
    JNINativeMethod mOnNewIntent[] = {
      {
        "_onNewIntent",
        "(Landroid/content/Intent;)V",
        (void*)&CJNIContext::_onNewIntent
      },
      {
        "_SetVideoPlayViewHolder",
        "(Landroid/view/SurfaceHolder;)V",
        (void*)&CJNIContext::_SetVideoPlayViewHolder
      },
      {
        "_SetVideoPlaySurface",
        "(Landroid/view/Surface;)V",
        (void*)&CJNIContext::_SetVideoPlaySurface
      }  
    };
    env->RegisterNatives(cMain, mOnNewIntent, sizeof(mOnNewIntent)/sizeof(mOnNewIntent[0]));
  }

  jclass cBroadcastReceiver = env->FindClass(bcReceiver.c_str());
  if(cBroadcastReceiver)
  {
    JNINativeMethod mOnReceive =  {
      "_onReceive",
      "(Landroid/content/Intent;)V",
      (void*)&CJNIBroadcastReceiver::_onReceive
    };
    env->RegisterNatives(cBroadcastReceiver, &mOnReceive, 1);
  }

  jclass cFrameAvailableListener = env->FindClass(frameListener.c_str());
  if(cFrameAvailableListener)
  {
    JNINativeMethod mOnFrameAvailable = {
      "_onFrameAvailable",
      "(Landroid/graphics/SurfaceTexture;)V",
      (void*)&CJNISurfaceTextureOnFrameAvailableListener::_onFrameAvailable
    };
    env->RegisterNatives(cFrameAvailableListener, &mOnFrameAvailable, 1);
  }

  return version;
}
