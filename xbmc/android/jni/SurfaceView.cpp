#include "JNIBase.h"
#include "Context.h"
#include "ClassLoader.h"
#include "SurfaceView.h"
#include "jutils/jutils-details.hpp"
#include <algorithm>


using namespace jni;
CJNISurfaceView::CJNISurfaceView():CJNIBase("android/view/SurfaceHolder")
{
	m_object = CJNIContext::getSurfaceViewHolder();
	m_object.setGlobal();
}

CJNISurfaceFromSurfaceView  CJNISurfaceView::getSurface()
{
	return call_method<jhobject>(m_object, "getSurface", "()Landroid/view/Surface;");
}























