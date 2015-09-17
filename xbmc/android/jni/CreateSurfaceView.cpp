#include "JNIBase.h"
#include "Context.h"
#include "ClassLoader.h"
#include "CreateSurfaceView.h"
#include "jutils/jutils-details.hpp"
#include <algorithm>


using namespace jni;
CJNICreateSurfaceView::CJNICreateSurfaceView():CJNIBase("org/xbmc/kodi/Main")
{
}

void  CJNICreateSurfaceView::CreateSurface()
{
	call_method<jhobject>(CJNIContext::getContext(), "CreateSurface", "()V");
}

void  CJNICreateSurfaceView::DeleteSurface()
{

	call_method<jhobject>(CJNIContext::getContext(), "DeleteSurface", "()V");
}
























