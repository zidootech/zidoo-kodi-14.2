#ifndef __SURFACEVIEW_H__
#define __SURFACEVIEW_H__
#include "JNIBase.h"

class  CJNISurfaceFromSurfaceView : public CJNIBase
{
public:
   CJNISurfaceFromSurfaceView(const jni::jhobject &object) : CJNIBase(object) {m_object.setGlobal();};
  ~CJNISurfaceFromSurfaceView() {};
};

class CJNISurfaceView : public CJNIBase
{
public:
	CJNISurfaceView();
    
    ~CJNISurfaceView(){};
	CJNISurfaceFromSurfaceView  getSurface();
private:
    jni::jhobject m_context;
};
#endif
