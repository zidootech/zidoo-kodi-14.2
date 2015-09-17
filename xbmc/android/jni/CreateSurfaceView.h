#ifndef __CREATESURFACEVIEW_H__
#define __CREATESURFACEVIEW_H__
#include "JNIBase.h"

class CJNICreateSurfaceView : public CJNIBase
{
public:
	CJNICreateSurfaceView();
    
    ~CJNICreateSurfaceView(){};
	void CreateSurface();
	void DeleteSurface();
};
#endif
