/*
 *      Copyright (C) 2005-2013 Team XBMC
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

#include "system.h"

#include "AWCodec.h"
#include "DynamicDll.h"

#include "threads/SingleLock.h"
#include "cores/dvdplayer/DVDClock.h"
#include "cores/VideoRenderers/RenderFlags.h"
#include "cores/VideoRenderers/RenderManager.h"
//#include "guilib/GraphicContext.h"
#include "settings/MediaSettings.h"
#include "settings/Settings.h"
//#include "utils/AMLUtils.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/TimeUtils.h"

#if defined(TARGET_ANDROID)
#include "android/activity/AndroidFeatures.h"
#include "utils/BitstreamConverter.h"
#include "android/jni/SurfaceView.h"
#include "android/jni/Context.h"
#endif

#define USE_DETNTERLACE 1


#include <unistd.h>
#include <queue>
#include <vector>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>


class DllLibawCodecInterface
{
public:
  virtual ~DllLibawCodecInterface(){};

  virtual VideoDecoder* CreateVideoDecoder(void) = 0;

  virtual void DestroyVideoDecoder(VideoDecoder* pDecoder) = 0;

  virtual int InitializeVideoDecoder(VideoDecoder* pDecoder, VideoStreamInfo* pVideoInfo, VConfig* pVconfig) = 0;
  //*

  virtual void ResetVideoDecoder(VideoDecoder* pDecoder) = 0;

  virtual int DecodeVideoStream(VideoDecoder* pDecoder, 
			                      int           bEndOfStream,
			                      int           bDecodeKeyFrameOnly,
			                      int           bDropBFrameIfDelay,
			                      int64_t       nCurrentTimeUs) = 0;

  virtual int GetVideoStreamInfo(VideoDecoder* pDecoder, VideoStreamInfo* pVideoInfo) = 0;

  virtual int RequestVideoStreamBuffer(VideoDecoder* pDecoder,
			                             int           nRequireSize,
			                             char**        ppBuf,
			                             int*          pBufSize,
			                             char**        ppRingBuf,
			                             int*          pRingBufSize,
			                             int           nStreamBufIndex) = 0;

  virtual int SubmitVideoStreamData(VideoDecoder*        pDecoder,
		                          VideoStreamDataInfo* pDataInfo,
		                          int                  nStreamBufIndex) = 0;

  virtual int VideoStreamBufferSize(VideoDecoder* pDecoder, int nStreamBufIndex) = 0;

  virtual int VideoStreamDataSize(VideoDecoder* pDecoder, int nStreamBufIndex) = 0;

  virtual int VideoStreamFrameNum(VideoDecoder* pDecoder, int nStreamBufIndex) = 0;

  virtual void* VideoStreamDataInfoPointer(VideoDecoder* pDecoder, int nStreamBufIndex) = 0;

  virtual VideoPicture* RequestPicture(VideoDecoder* pDecoder, int nStreamIndex) = 0;

  virtual int ReturnPicture(VideoDecoder* pDecoder, VideoPicture* pPicture) = 0;

  virtual VideoPicture* NextPictureInfo(VideoDecoder* pDecoder, int nStreamIndex) = 0;

  virtual int TotalPictureBufferNum(VideoDecoder* pDecoder, int nStreamIndex) = 0;

  virtual int EmptyPictureBufferNum(VideoDecoder* pDecoder, int nStreamIndex) = 0;

  virtual int ValidPictureNum(VideoDecoder* pDecoder, int nStreamIndex) = 0;

  virtual int ConfigHorizonScaleDownRatio(VideoDecoder* pDecoder, int nScaleDownRatio) = 0;

  virtual int ConfigVerticalScaleDownRatio(VideoDecoder* pDecoder, int nScaleDownRatio) = 0;

  virtual int ConfigRotation(VideoDecoder* pDecoder, int nRotateDegree) = 0;

  virtual int ConfigDeinterlace(VideoDecoder* pDecoder, int bDeinterlace) = 0;

  virtual int ConfigThumbnailMode(VideoDecoder* pDecoder, int bOpenThumbnailMode) = 0;

  virtual int ConfigOutputPicturePixelFormat(VideoDecoder* pDecoder, int ePixelFormat) = 0;

  virtual int ConfigNoBFrames(VideoDecoder* pDecoder, int bNoBFrames) = 0;

  virtual int ConfigDisable3D(VideoDecoder* pDecoder, int bDisable3D) = 0;

  virtual int ConfigVeMemoryThresh(VideoDecoder* pDecoder, int nMemoryThresh) = 0;

  virtual int ReopenVideoEngine(VideoDecoder* pDecoder, VConfig* pVConfig, VideoStreamInfo* pStreamInfo) = 0;

  virtual int RotatePicture(VideoPicture* pPictureIn, 
							VideoPicture* pPictureOut, 
							int 		  nRotateDegree,
							int 		  nGpuYAlign,
							int 		  nGpuCAlign) = 0;

  virtual int RotatePictureHw(VideoDecoder* pDecoder,
							  VideoPicture* pPictureIn, 
							  VideoPicture* pPictureOut, 
							  int			nRotateDegree) = 0;

  virtual VideoPicture* AllocatePictureBuffer(int nWidth, int nHeight, int nLineStride, int ePixelFormat) = 0;

  virtual int FreePictureBuffer(VideoPicture* pPicture) = 0;

  //virtual int SetDecoderCallback(VideoDecoder* pDecoder, Callback callback, void* pUserData) = 0;

  virtual char* VideoRequestSecureBuffer(VideoDecoder* pDecoder,int nBufferSize) = 0;

  virtual void VideoReleaseSecureBuffer(VideoDecoder* pDecoder,char* pBuf) = 0;
//*/
};

class DllLibAwCodec : public DllDynamic, DllLibawCodecInterface
{
  // libawcodec is static linked into libvdecoder.so
  DECLARE_DLL_WRAPPER(DllLibAwCodec, "libvdecoder.so")
  //LOAD_SYMBOLS()
  DEFINE_METHOD0(VideoDecoder*, 	CreateVideoDecoder)
  DEFINE_METHOD1(void,			DestroyVideoDecoder,			(VideoDecoder* p1))
  DEFINE_METHOD3(int, 			InitializeVideoDecoder,			(VideoDecoder* p1, VideoStreamInfo* p2, VConfig* p3))
  DEFINE_METHOD1(void,			ResetVideoDecoder,				(VideoDecoder* p1))
  DEFINE_METHOD5(int,				DecodeVideoStream,				(VideoDecoder* p1, int p2, int p3, int p4, int64_t p5))
  DEFINE_METHOD2(int,				GetVideoStreamInfo,				(VideoDecoder* p1, VideoStreamInfo* p2))
  DEFINE_METHOD7(int,				RequestVideoStreamBuffer,		(VideoDecoder* p1, int p2, char** p3, int* p4, char** p5, int* p6, int p7))
  DEFINE_METHOD3(int,				SubmitVideoStreamData,			(VideoDecoder* p1, VideoStreamDataInfo* p2, int p3))
  DEFINE_METHOD2(int,				VideoStreamBufferSize,			(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(int,				VideoStreamDataSize,			(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(int,				VideoStreamFrameNum,			(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(void*,			VideoStreamDataInfoPointer,		(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(VideoPicture*,	RequestPicture,					(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(int,				ReturnPicture,					(VideoDecoder* p1, VideoPicture* p2))
  DEFINE_METHOD2(VideoPicture*,	NextPictureInfo,				(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(int,				TotalPictureBufferNum,			(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(int,				EmptyPictureBufferNum,			(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(int,				ValidPictureNum,				(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(int,				ConfigHorizonScaleDownRatio,	(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(int,				ConfigVerticalScaleDownRatio,	(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(int,				ConfigRotation,					(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(int,				ConfigDeinterlace,				(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(int,				ConfigThumbnailMode,			(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(int,				ConfigOutputPicturePixelFormat,	(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(int,				ConfigNoBFrames,				(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(int,				ConfigDisable3D,				(VideoDecoder* p1, int p2))
  DEFINE_METHOD2(int,				ConfigVeMemoryThresh,			(VideoDecoder* p1, int p2))
  DEFINE_METHOD3(int,				ReopenVideoEngine,				(VideoDecoder* p1, VConfig* p2, VideoStreamInfo* p3))
  DEFINE_METHOD5(int,				RotatePicture,					(VideoPicture* p1,VideoPicture* p2,int p3,int p4,int p5))
  DEFINE_METHOD4(int,				RotatePictureHw,				(VideoDecoder* p1, VideoPicture* p2, VideoPicture* p3, int p4))
  DEFINE_METHOD4(VideoPicture*,	AllocatePictureBuffer,			(int p1, int p2, int p3, int p4))
  DEFINE_METHOD1(int,				FreePictureBuffer,				(VideoPicture* p1))
  //DEFINE_METHOD3(int,				SetDecoderCallback,				(VideoDecoder* p1, Callback p2, void* p3))
  DEFINE_METHOD2(char*,			VideoRequestSecureBuffer,		(VideoDecoder* p1,int p2))
  DEFINE_METHOD2(void,			VideoReleaseSecureBuffer,		(VideoDecoder* p1,char* p2))

  BEGIN_METHOD_RESOLVE()
    RESOLVE_METHOD(CreateVideoDecoder)
    RESOLVE_METHOD(DestroyVideoDecoder)
    RESOLVE_METHOD(InitializeVideoDecoder)
    //*
    RESOLVE_METHOD(ResetVideoDecoder)
    RESOLVE_METHOD(DecodeVideoStream)
    RESOLVE_METHOD(GetVideoStreamInfo)
    RESOLVE_METHOD(RequestVideoStreamBuffer)
    RESOLVE_METHOD(SubmitVideoStreamData)
    RESOLVE_METHOD(VideoStreamBufferSize)
    RESOLVE_METHOD(VideoStreamDataSize)
    RESOLVE_METHOD(VideoStreamFrameNum)
    RESOLVE_METHOD(VideoStreamDataInfoPointer)
    RESOLVE_METHOD(RequestPicture)
    RESOLVE_METHOD(ReturnPicture)
    RESOLVE_METHOD(NextPictureInfo)
	RESOLVE_METHOD(TotalPictureBufferNum)
	RESOLVE_METHOD(EmptyPictureBufferNum)
	RESOLVE_METHOD(ValidPictureNum)
	RESOLVE_METHOD(ConfigHorizonScaleDownRatio)
	RESOLVE_METHOD(ConfigVerticalScaleDownRatio)
	RESOLVE_METHOD(ConfigRotation)
	RESOLVE_METHOD(ConfigDeinterlace)
	RESOLVE_METHOD(ConfigThumbnailMode)
	RESOLVE_METHOD(ConfigOutputPicturePixelFormat)
	RESOLVE_METHOD(ConfigNoBFrames)
	RESOLVE_METHOD(ConfigDisable3D)
	RESOLVE_METHOD(ConfigVeMemoryThresh)
	RESOLVE_METHOD(ReopenVideoEngine)
    RESOLVE_METHOD(RotatePicture)
	RESOLVE_METHOD(RotatePictureHw)
	RESOLVE_METHOD(AllocatePictureBuffer)
	RESOLVE_METHOD(FreePictureBuffer)
	//RESOLVE_METHOD(SetDecoderCallback)
	RESOLVE_METHOD(VideoRequestSecureBuffer)
	RESOLVE_METHOD(VideoReleaseSecureBuffer)
	//*/
  END_METHOD_RESOLVE()

public:
	void test()
	{
		CLog::Log(0,"hello");	
	}
  
};

typedef int (*PlayerCallback)(void* pUserData, VideoPicture* picture);

class DllLibawRenderInterface
{
public:
  virtual ~DllLibawRenderInterface(){};
  virtual RenderContext* RenderInit(void *jniEnv, jobject pNativeWindow) = 0;
  virtual int RenderDeinit(RenderContext * pRenderContext) = 0;
  virtual int RenderPicture(RenderContext *pRenderContext, VideoPicture* pPicture) = 0;
  virtual int RenderQueueBuffer(RenderContext* r, VideoPicture* pBuf, int bValid) = 0;
  virtual int RenderDequeueBuffer(RenderContext* r, VideoPicture** ppBuf) = 0;
  virtual int RenderSetRenderToHardwareFlag(RenderContext* r, int bFlag) = 0;
  virtual int RenderSetDeinterlaceFlag(RenderContext* r, int bFlag) = 0;
  virtual int RenderSetExpectPixelFormat(RenderContext* r, enum EPIXELFORMAT ePixelFormat) = 0;  
  virtual int RenderSetPictureSize(RenderContext *apRenderContext, int nWidth, int nHeight) = 0;
  virtual int RenderSetDisplayRegion(RenderContext* r, int nLeftOff, int nTopOff, int nDisplayWidth, int nDisplayHeight) = 0;  
  virtual int RenderSetBufferTimeStamp(RenderContext* apRenderContext, int64_t nPtsAbs) = 0;
  virtual int SetCallback(RenderContext * apRenderContext, PlayerCallback callback, void *pUserData) = 0;
  virtual int GetDispFps(RenderContext *apRenderContext) = 0;
  virtual int RenderDeinterlaceInit(RenderContext *r) = 0;
  virtual int RenderDeinterlaceGetFlag(RenderContext *r) = 0;
  virtual int RenderDeinterlaceGetExpectPixelFormat(RenderContext *r) = 0;
  virtual int RenderDeinterlacProcess(RenderContext *r, 
                                                                          VideoPicture *pPrePicture, 
                                                                          VideoPicture *pCurPicture, 
                                                                          VideoPicture *pOutPicture, 
                                                                          int nField) = 0;
  virtual int RenderDeinterlaceReset(RenderContext *r) = 0;
};

class DllLibAwRender: public DllDynamic, DllLibawRenderInterface
{
  // libvrender is static linked into libvrender.so
  DECLARE_DLL_WRAPPER(DllLibAwRender, "libvrender.so")
  //LOAD_SYMBOLS()
  DEFINE_METHOD2(RenderContext*, 	RenderInit, (void *p1, jobject p2))
  DEFINE_METHOD1(int, 	RenderDeinit, (RenderContext *p1))
  DEFINE_METHOD2(int, 	RenderPicture, (RenderContext *p1, VideoPicture *p2))
  DEFINE_METHOD3(int, 	RenderQueueBuffer, (RenderContext *p1, VideoPicture *p2, int p3))
  DEFINE_METHOD2(int, 	RenderDequeueBuffer, (RenderContext *p1, VideoPicture **p2))
  DEFINE_METHOD2(int, 	RenderSetRenderToHardwareFlag, (RenderContext *p1, int p2))
  DEFINE_METHOD2(int, 	RenderSetDeinterlaceFlag, (RenderContext *p1, int p2))  
  DEFINE_METHOD2(int, 	RenderSetExpectPixelFormat, (RenderContext *p1, enum EPIXELFORMAT p2))  
  DEFINE_METHOD3(int, 	RenderSetPictureSize, (RenderContext *p1, int p2, int p3))    
  DEFINE_METHOD5(int, 	RenderSetDisplayRegion, (RenderContext *p1, int p2, int p3, int p4, int p5))    
  DEFINE_METHOD2(int, 	RenderSetBufferTimeStamp, (RenderContext *p1, int64_t p2))  
  DEFINE_METHOD3(int, 	SetCallback, (RenderContext *p1, PlayerCallback p2, void *p3))   
  DEFINE_METHOD1(int, 	GetDispFps, (RenderContext *p1))
  DEFINE_METHOD1(int, 	RenderDeinterlaceInit, (RenderContext *p1))  
  DEFINE_METHOD1(int, 	RenderDeinterlaceGetFlag, (RenderContext *p1))  
  DEFINE_METHOD1(int, 	RenderDeinterlaceGetExpectPixelFormat, (RenderContext *p1))  
  DEFINE_METHOD5(int, 	RenderDeinterlacProcess, (RenderContext *p1, VideoPicture *p2, VideoPicture *p3, VideoPicture *p4, int p5))  
  DEFINE_METHOD1(int, 	RenderDeinterlaceReset, (RenderContext *p1))  
  BEGIN_METHOD_RESOLVE()
  RESOLVE_METHOD(RenderInit)
  RESOLVE_METHOD(RenderDeinit)
  RESOLVE_METHOD(RenderPicture)
  RESOLVE_METHOD(RenderQueueBuffer)
  RESOLVE_METHOD(RenderDequeueBuffer)
  RESOLVE_METHOD(RenderSetRenderToHardwareFlag)
  RESOLVE_METHOD(RenderSetDeinterlaceFlag)
  RESOLVE_METHOD(RenderSetExpectPixelFormat)    
  RESOLVE_METHOD(RenderSetPictureSize)  
  RESOLVE_METHOD(RenderSetDisplayRegion)    
  RESOLVE_METHOD(RenderSetBufferTimeStamp)  
  RESOLVE_METHOD(SetCallback)  
  RESOLVE_METHOD(GetDispFps)  
  RESOLVE_METHOD(RenderDeinterlaceInit)  
  RESOLVE_METHOD(RenderDeinterlaceGetFlag)  
  RESOLVE_METHOD(RenderDeinterlaceGetExpectPixelFormat)  
  RESOLVE_METHOD(RenderDeinterlacProcess)  
  RESOLVE_METHOD(RenderDeinterlaceReset)  
  END_METHOD_RESOLVE()
};


static VideoCodecFormat_t AVCodecIDToAwID(enum AVCodecID id)
{
  VideoCodecFormat_t format;
  switch (id)
  {
    case AV_CODEC_ID_MPEG1VIDEO:
		format = VIDEO_CODEC_FORMAT_MPEG1;
		break;
    case AV_CODEC_ID_MPEG2VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO_XVMC:
		format = VIDEO_CODEC_FORMAT_MPEG2;
		break;
    case AV_CODEC_ID_H263:
		format = VIDEO_CODEC_FORMAT_H263;
		break;
    case AV_CODEC_ID_MPEG4:
		//format = VIDEO_CODEC_FORMAT_MPEG4;
		format = VIDEO_CODEC_FORMAT_DIVX5;
		break;
    case AV_CODEC_ID_H263P:
    case AV_CODEC_ID_H263I:
    case AV_CODEC_ID_MSMPEG4V2:
      break;
    case AV_CODEC_ID_MSMPEG4V3:
      format = VIDEO_CODEC_FORMAT_DIVX3;
      break;
    case AV_CODEC_ID_FLV1:
		format = VIDEO_CODEC_FORMAT_SORENSSON_H263;
		break;
    case AV_CODEC_ID_RV10:
    case AV_CODEC_ID_RV20:
		format = VIDEO_CODEC_FORMAT_RX;
		break;
    case AV_CODEC_ID_RV30:
    case AV_CODEC_ID_RV40:
		format = VIDEO_CODEC_FORMAT_RX;
		break;
    case AV_CODEC_ID_H264:
		format = VIDEO_CODEC_FORMAT_H264;
		break;
	/*
    case AV_CODEC_ID_H264MVC:
      // H264 Multiview Video Coding (3d blurays)
      format = VIDEO_CODEC_FORMAT_H264;
      break;
    */
    case AV_CODEC_ID_MJPEG:
      format = VIDEO_CODEC_FORMAT_MJPEG;
      break;
    case AV_CODEC_ID_VC1:
    case AV_CODEC_ID_WMV3:
      format = VIDEO_CODEC_FORMAT_WMV3;
      break;
    case AV_CODEC_ID_AVS:
    case AV_CODEC_ID_CAVS:
      format = VIDEO_CODEC_FORMAT_AVS;
      break;
    case AV_CODEC_ID_HEVC:
      format = VIDEO_CODEC_FORMAT_H265;
      break;
    default:
      format = VIDEO_CODEC_FORMAT_UNKNOWN;
      break;
  }

  CLog::Log(LOGDEBUG, "AVCodecIDToAwID, AVCodecID(%d) -> AwID(0x%x)", (int)id, format);
  return format;
}


CAWCodec::CAWCodec() : CThread("CAWCodec")
{
  m_opened = false;
  aw_private = new aw_private_t;
  memset(aw_private, 0, sizeof(aw_private_t));
  m_dll = new DllLibAwCodec;
  m_dll->Load();
   aw_private->m_dll = m_dll;

  m_render_dll = new DllLibAwRender;
  m_render_dll->Load();
  
  bConfigDropDelayFrames = 0;
  m_do_reset = false;
  mRenderContext = NULL;
  m_surface_relased = true;
  m_appState = 1;
   memset(&m_streamInfo, 0, sizeof(VideoStreamInfo));
   m_streamInputFormat = NULL;
}


CAWCodec::~CAWCodec()
{
  StopThread();
  if (aw_private->pVideoCodec) 
  {
    m_dll->DestroyVideoDecoder(aw_private->pVideoCodec);
    aw_private->pVideoCodec = NULL;
  }  
  delete aw_private;
  aw_private = NULL;
  delete m_dll, m_dll = NULL;
  delete m_render_dll, m_render_dll = NULL;
}

bool CAWCodec::OpenDecoder(CDVDStreamInfo &hints)
{
#ifdef TARGET_ANDROID
  CLog::Log(LOGDEBUG, "CAWCodec::OpenDecoder, android version %d", CAndroidFeatures::GetVersion());
#endif

  m_hints = hints;

  VConfig 			vConfig;
  //VideoCodecFormat_t vCodecFormat;
  memset(&vConfig, 0, sizeof(VConfig));
  //initQueue();


  aw_private->vCodecFormat = AVCodecIDToAwID(hints.codec);
  if(aw_private->vCodecFormat == VIDEO_CODEC_FORMAT_UNKNOWN)
  {
	CLog::Log(LOGDEBUG, "CAWCodec::OpenDecoder not Support CodecFormat");
	return false;
  }
  
  m_streamInfo.nWidth = hints.width;
  m_streamInfo.nHeight = hints.height;
  m_streamInfo.eCodecFormat = aw_private->vCodecFormat;
  m_streamInfo.bIs3DStream = 0;

  m_streamInfo.pCodecSpecificData = NULL;

  CLog::Log(LOGDEBUG, "CAWCodec::OpenDecoder fpsrate[%d], fpsscale[%d], rfpsrate[%d], rfpsscale[%d]", hints.fpsrate, hints.fpsscale, hints.rfpsrate, hints.rfpsscale);
  if ( hints.fpsrate > 0)
    m_streamInfo.nFrameRate = hints.fpsrate;

  // handle video rate
  if (hints.rfpsrate > 0 && hints.rfpsscale != 0)
  {
    // check ffmpeg r_frame_rate 1st
    m_streamInfo.nFrameRate =hints.rfpsrate * 1000 / hints.rfpsscale; 
  }
  else if (hints.fpsrate > 0 && hints.fpsscale != 0)
  {
    // then ffmpeg avg_frame_rate next
    m_streamInfo.nFrameRate =hints.fpsrate * 1000 / hints.fpsscale; 
  }
  
  CLog::Log(LOGDEBUG, "CAWCodec::OpenDecoder nFrameRate = %d", m_streamInfo.nFrameRate);
  if (m_streamInfo.nFrameRate > 0) 
  {
    m_streamInfo.nFrameDuration = 1000000000 / m_streamInfo.nFrameRate;
  }
  CLog::Log(LOGDEBUG, "CAWCodec::OpenDecoder nFrameDuration = %d", m_streamInfo.nFrameDuration);

  if (hints.width >= 3840 && hints.height >= 2160)
  {
      // decoder drop delay frame
      bConfigDropDelayFrames = 1;
      CLog::Log(LOGDEBUG, "CAWCodec::OpenDecoder 4K -> config drop delay frame, and enable scale down");
      // decoder config scale down, refrence to awplayer H3 scale down when 4k
      vConfig.bScaleDownEn = 1;
      vConfig.nHorizonScaleDownRatio = 1;
      vConfig.nVerticalScaleDownRatio = 1;
  }
  else 
  {
     bConfigDropDelayFrames = 0;
  }
  
  #if 1
  // handle codec extradata

  int useExtradata = 1;
  if (hints.codec == AV_CODEC_ID_VC1) 
  {
    if ((m_streamInputFormat != NULL) && (strcmp((const char*)m_streamInputFormat, "mpegts") == 0)) 
    {
        //ignore extra data for transport stream when codec is VC1.
        //H3 decoder does not need the specific data for VC1. it should get bad if using it.
        useExtradata = 0;
        CLog::Log(LOGDEBUG, "CAWCodec::OpenDecoder : VC1 is transport stream, ignore Specific Data");
    }
  }
  
  if ((m_hints.extrasize > 0) && (useExtradata == 1))
  {
    size_t size = m_hints.extrasize;
    void  *src_ptr = m_hints.extradata;
    /*
    if (m_bitstream)
    {
      size = m_bitstream->GetExtraSize();
      src_ptr = m_bitstream->GetExtraData();
    }
    */
    m_streamInfo.pCodecSpecificData = (char*)malloc(size);
    if (m_streamInfo.pCodecSpecificData != NULL) 
    {
      memcpy(m_streamInfo.pCodecSpecificData, src_ptr, size);
      m_streamInfo.nCodecSpecificDataLen = size;
      /*
      // dump specific data
      int i = 0;
      for (i = 0; i < m_streamInfo.nCodecSpecificDataLen; i++) 
      {
        CLog::Log(LOGDEBUG, "specificData[%d] = %d ", i, m_streamInfo.pCodecSpecificData[i]);
      }
      */
    }
  }
  else 
  {  
    m_streamInfo.nCodecSpecificDataLen = 0;
    m_streamInfo.pCodecSpecificData = NULL;
  }
  #endif

  if(m_streamInfo.eCodecFormat == VIDEO_CODEC_FORMAT_WMV3)
  {
    CLog::Log(LOGDEBUG, " m_streamInfo.bIsFramePackage to 1 when it is vc1");
    m_streamInfo.bIsFramePackage = 1;
  }

  //* Set VideoDecode OutputPixelFormat YV12
  vConfig.eOutputPixelFormat  = aw_private->PixelFormat = PIXEL_FORMAT_NV21; //PIXEL_FORMAT_YV12;
  //PIXEL_FORMAT_YUV_PLANER_420  PIXEL_FORMAT_NV12

  aw_private->pVideoCodec = m_dll->CreateVideoDecoder();
  if(aw_private->pVideoCodec == NULL)
  {
	  CLog::Log(LOGERROR,"CAWCodec::OpenDecoder CreateVideoDecoder fail\n");
	  return false;
  }
  
  CLog::Log(LOGDEBUG,"CAWCodec::OpenDecoder  nWidth*nHeight[%d*%d]  CodecFormat[0x%x] OutputPixelFormat[%d]",
    m_streamInfo.nWidth,m_streamInfo.nHeight,m_streamInfo.eCodecFormat,aw_private->PixelFormat);
  
  m_dll->InitializeVideoDecoder(aw_private->pVideoCodec, &m_streamInfo, &vConfig);



  aw_private->nDecodeFrameCount = 0;
  m_speed = DVD_PLAYSPEED_NORMAL;
  SetSpeed(m_speed);
  m_do_reset = false;
  
  Create();
  
  m_opened = true;


  return true;
}

void CAWCodec::CloseDecoder()
{
  CLog::Log(LOGDEBUG, "CAWCodec::CloseDecoder");
  StopThread();
  
  // release surface before destory video code
  // becaure render release may rebutn picture to video codec
  ReleaseSurface();

  if (m_streamInfo.pCodecSpecificData != NULL) {
      free(m_streamInfo.pCodecSpecificData);
      m_streamInfo.pCodecSpecificData = NULL;
      m_streamInfo.nCodecSpecificDataLen = 0;
  }
  
  if (aw_private->pVideoCodec) 
  {
    m_dll->DestroyVideoDecoder(aw_private->pVideoCodec);
    aw_private->pVideoCodec = NULL;
  }

  m_surface_relased = true;
  m_opened = false;

}

void CAWCodec::Reset()
{
  CLog::Log(LOGDEBUG, "CAWCodec::Reset "); 
  if (!m_opened)
    return;

  m_dll->ResetVideoDecoder(aw_private->pVideoCodec);
  m_do_reset = true;
}

int CAWCodec::RendCallBack(void* pUserData, VideoPicture* picture)
{
    CAWCodec *p = (CAWCodec*)pUserData;
    if(picture == NULL)
        return 0;

    //CLog::Log(LOGERROR, "SubmitVideoStreamData() error!");
    if (p && p->m_dll && p->aw_private && p->aw_private->pVideoCodec) 
    {
        p->m_dll->ReturnPicture(p->aw_private->pVideoCodec, picture);	
    }

    return 0;
}

int CAWCodec::Decode(uint8_t *pData, size_t iSize, double dts, double pts)
{
    if (!m_opened)
        return VC_BUFFER;

    int rtn = VC_BUFFER;
    int ret,nValidSize, nRequestDataSize;
    int endofstream = 0;
    int dropBFrameifdelay = bConfigDropDelayFrames;
    int64_t currenttimeus = -1;
    int64_t nPts = -1;   // local pts
    int decodekeyframeonly = 0;
    int streambufIndex = 0;    
    int pBufferState = 0;//0:no data input; 1:no streambuffer; 2:buffer ok    
    nRequestDataSize = iSize;
    int fillFailCount = 0;
    
    //CLog::Log(LOGDEBUG, "Decode: pData[%d], iSize[%d], dts[%f], pts[%f]", pData, iSize, dts, pts);
    if (m_hints.ptsinvalid)
        pts = DVD_NOPTS_VALUE; 

    if (pts != DVD_NOPTS_VALUE)
        nPts = pts;
    else if (dts!= DVD_NOPTS_VALUE)
        nPts = dts;

    if (pData == NULL && iSize <= 0) {
        pBufferState = 0;
        goto __DECODE;
    }


  
  /*	  
  ** step 1:  RequestVideoStreamBuffer()  //Get buffer from decoder
  ** step 2:  Fill oneFrame VideoDate in this buffer
  ** step 3:  SubmitVideoStreamData()  // Submit buffer to decoder  
  */

__FILLBUFFER:

        void*               pBuf0;
        void*               pBuf1;
        int                 nBufSize0;
        int                 nBufSize1;
        
        ret = m_dll->RequestVideoStreamBuffer(aw_private->pVideoCodec,
        							nRequestDataSize,
        							(char**)&pBuf0,
        							&nBufSize0,
        							(char**)&pBuf1,
        							&nBufSize1,
        							0);
        if(ret < 0 || (nBufSize0+nBufSize1) < nRequestDataSize)
        {
        	//CLog::Log(LOGERROR," RequestVideoStreamBuffer fail. request size: %d, valid size: %d ",nRequestDataSize, nValidSize);
		    pBufferState = 1;
                  fillFailCount++;
		    goto __DECODE;
        }
        fillFailCount = 0;
        
        if(nRequestDataSize > nBufSize0)
        {
          memcpy(pBuf0, pData, nBufSize0);
          memcpy(pBuf1, pData + nBufSize0, nRequestDataSize-nBufSize0);
        }
        else
          memcpy(pBuf0, pData, nRequestDataSize);

        /*
        if (pts != DVD_NOPTS_VALUE) {
          nPts = pts;
        }
        */
        //CLog::Log(LOGDEBUG, "submitStreamdata pts[ %lld ]", nPts);
        
        memset(&aw_private->dataInfo, 0, sizeof(VideoStreamDataInfo));	  
        aw_private->dataInfo.pData		= (char*)pBuf0;	  
        aw_private->dataInfo.nLength		= nRequestDataSize;	  
        aw_private->dataInfo.nPts 		= nPts; 
        aw_private->dataInfo.nPcr		= 0;
        aw_private->dataInfo.bIsFirstPart =  1;
        aw_private->dataInfo.bIsLastPart = 1;	
        if (nPts != -1) {
            aw_private->dataInfo.bValid = 1;
        } else {
            aw_private->dataInfo.bValid = 0;
        }

        do{
            int rep_count = 0;
            ret = m_dll->SubmitVideoStreamData(aw_private->pVideoCodec , &aw_private->dataInfo, 0);
            if (ret != 0)
            {
                rep_count ++;
                if (rep_count > 5) {			
                    CLog::Log(LOGERROR, "SubmitVideoStreamData() error!");
                    return VC_ERROR;
                }
                usleep(5);
            }
            else {
                break;
            }
        } while(1);
        
	pBufferState = 2;
	nRequestDataSize = 0;

	
__DECODE:
    currenttimeus = GetPlayerPts();
    ret = m_dll->DecodeVideoStream(aw_private->pVideoCodec,
    	   						endofstream,
    	   						decodekeyframeonly,
    	   						dropBFrameifdelay,
    	   						currenttimeus);
    //CLog::Log(LOGDEBUG, "DecodeVideoStream %d", ret);

    switch (ret) 
    {
      case VDECODE_RESULT_KEYFRAME_DECODED:
      //logd("Key frame decoded!\n");
      case VDECODE_RESULT_OK:
      case VDECODE_RESULT_FRAME_DECODED: 
      case VDECODE_RESULT_NO_FRAME_BUFFER:
      {       
        rtn = VC_PICTURE;
        if ((pBufferState == 0)||(pBufferState == 2)) {
        int num = m_dll->VideoStreamFrameNum(aw_private->pVideoCodec, 0 );
        if (num >= 2) {
          usleep(5000);
          goto __DECODE;
        }
        else
          rtn |= VC_BUFFER;
		}
		else if(pBufferState == 1)
			goto __FILLBUFFER;
		break;
      }
      case VDECODE_RESULT_CONTINUE:
		CLog::Log(LOGDEBUG,"No picture output!\n");
		rtn = VC_BUFFER;
		goto __DECODE;
		break;
      case VDECODE_RESULT_NO_BITSTREAM:
		CLog::Log(LOGDEBUG,"No bitstream!\n");
		rtn = VC_BUFFER;
		break;
      case VDECODE_RESULT_RESOLUTION_CHANGE:
		CLog::Log(LOGDEBUG,"Resolution change!\n");
		break;
      case VDECODE_RESULT_UNSUPPORTED:
		CLog::Log(LOGDEBUG,"Unsupported!\n");
		rtn = VC_ERROR;
      break;
      default:
		CLog::Log(LOGDEBUG,"video decode Error: %d!\n", ret);
		rtn = VC_ERROR;
      break;
    }

    if (pBufferState == 1) {
        usleep(15);
        if (fillFailCount > 30) { //FIXME: 30 is ok??
            CLog::Log(LOGDEBUG, "Decode: fill fail count expire\n");
            rtn = VC_ERROR;
        } else {
            CLog::Log(LOGDEBUG, "Decode: try fill again\n");
            goto __FILLBUFFER;
        }
    }

  return rtn;
}

bool CAWCodec::GetPicture(DVDVideoPicture *pDvdVideoPicture)
{
    if (!m_opened)
        return false;

    pDvdVideoPicture->pts = GetPlayerPts();// + m_streamInfo.nFrameDuration / 2000 ; //DVD_NOPTS_VALUE;
    pDvdVideoPicture->iFlags = DVP_FLAG_ALLOCATED;
    pDvdVideoPicture->format = RENDER_FMT_BYPASS;
    pDvdVideoPicture->dts = DVD_NOPTS_VALUE;


    return true;
}

void CAWCodec::SetSpeed(int speed)
{
    CLog::Log(LOGDEBUG, "CAWCodec::SetSpeed, speed(%d)", speed); 
    m_speed = speed;
}

int CAWCodec::GetDataSize()
{
  if (!m_opened)
    return 0;

  return 0;
}

double CAWCodec::GetTimeSize()
{
  if (!m_opened)
    return 0;


  return 0;
}

int CAWCodec::GetUndisplayNum()
{
	int ret = 0;

	ret = m_dll->VideoStreamFrameNum(aw_private->pVideoCodec, 0 ) + m_dll->ValidPictureNum(aw_private->pVideoCodec, 0);
	if(ret > 32)
		ret = 32;

	return ret;

}

static int64_t getSystemTime()
{
	struct timeval  mLastOsTime;
	int64_t time = 0;
	
	gettimeofday(&mLastOsTime, NULL);
	time = mLastOsTime.tv_sec*1000000+mLastOsTime.tv_usec;
	return time;
}

static int64_t ptsToAbs(int64_t pts, int64_t curTime)
{
	int64_t ret = 0;

	ret = (pts - curTime)+getSystemTime();
	return ret;
}

int CAWCodec::Wait(volatile bool& bStop, int timeout)
{
  CSingleLock lock2(m_presentlock);

  XbmcThreads::EndTime endtime(timeout);
  while(!bStop)
  {
    m_presentevent.wait(lock2, std::min(50, timeout));
    if(endtime.IsTimePast() || bStop)
    {
      if (timeout != 0 && !bStop)
      return -1;
    }
  }

  return 0;
}

void CAWCodec::Process()
{
    CLog::Log(LOGDEBUG, "CAWCodec::Process Started");
    int streambufIndex = 0;
    int64_t firstPts = 0;
    int64_t tempTime = 0;
    int64_t sleepTime = 0;
    int64_t dropTime = 0;
    int seekDropCount = 0;
    int bFirstPictureShowed = 0;
    VideoPicture* 		pPicture = NULL;
    VideoPicture*     pDeinterlacePrePicture = NULL;
    VideoPicture*     pRenderBuffer = NULL, *pPreRenderBuffer = NULL;
    int bDeinterlaceFlag = 0, nDeinterlaceDispNum = 0;
    int nRenderBufferMode = 0;
    int dropCount = 0;
    
    int isInitSurface = 0;
    jni::jhobject surface;
    
    SetPriority(THREAD_PRIORITY_ABOVE_NORMAL);
    while (!m_bStop)
    { 
        // wait for surface prepare
        while ((isInitSurface == 0) && (!m_bStop)){
            surface = CJNIContext::getSurface();
            if (surface.get() != 0 && m_appState != 0) {
                SetSurface((void*)xbmc_jnienv(), &surface);
                if (mRenderContext != NULL)
                    isInitSurface = 1;
            }
            Wait(m_bStop, 10);
        } 
        
__RENDER_START:
        //CLog::Log(LOGDEBUG, "Process : render start == ");
        pPicture = NULL;
        while ((pPicture == NULL)&&(!m_bStop)){
            pPicture = m_dll->RequestPicture(aw_private->pVideoCodec, 0);
            if (pPicture){
                break;
            }
            //wait for 5ms for request pictrue
            Wait(m_bStop, 5);
            //usleep(5000);
            if (!m_surface_relased) break;
        }
    
        while((m_speed == 0)&&(!m_bStop)&&(bFirstPictureShowed == 1)){
            seekDropCount = 1;
            Wait(m_bStop, 10);
            if (!m_surface_relased) break;
        }

        if (m_do_reset) 
        {
            m_do_reset = false;
            seekDropCount = 1;
        }

        if (m_bStop)
            break;

        if (pPicture != NULL && bFirstPictureShowed == 1 && seekDropCount) 
        {
            sleepTime = pPicture->nPts - GetPlayerPts();
            int delta = 500000;      
            if (bDeinterlaceFlag == 1)         
                delta = 250000;
            if (sleepTime > delta || sleepTime < -delta) {
                CLog::Log(LOGDEBUG, "Process : seek time, delta = %lld, drop picture", sleepTime);
                if (pPicture)
                { 
                    m_dll->ReturnPicture(aw_private->pVideoCodec, pPicture);
                    pPicture = NULL;
                }    
                if (pDeinterlacePrePicture != NULL)
                {
                    m_dll->ReturnPicture(aw_private->pVideoCodec,  pDeinterlacePrePicture);
                    pDeinterlacePrePicture = NULL;
                }     
                if (mRenderContext != NULL) {
                    if(pRenderBuffer != NULL)
                    {
                        m_render_dll->RenderQueueBuffer(mRenderContext, pRenderBuffer, 0);;     
                        pRenderBuffer = NULL;
                    }        
                    if(pPreRenderBuffer != NULL)
                    {
                        m_render_dll->RenderQueueBuffer(mRenderContext, pPreRenderBuffer, 0);
                        pPreRenderBuffer = NULL;
                    }
                }
                if (sleepTime > delta)
                    seekDropCount = 0;  // skip once
                continue;
            }	
            seekDropCount = 0;			
        }

        if (/*CJNIContext::isSurfaceChange()*/ !m_surface_relased)
        {
            if (pPicture)
            { 
                m_dll->ReturnPicture(aw_private->pVideoCodec, pPicture);
                pPicture = NULL;
            }   
            if (pDeinterlacePrePicture != NULL)
            {
                m_dll->ReturnPicture(aw_private->pVideoCodec,  pDeinterlacePrePicture);
                pDeinterlacePrePicture = NULL;
            }     
            if (mRenderContext != NULL) {
                if(pRenderBuffer != NULL)
                {
                    m_render_dll->RenderQueueBuffer(mRenderContext, pRenderBuffer, 0);
                    pRenderBuffer = NULL;
                }             
                if(pPreRenderBuffer != NULL)
                {
                    m_render_dll->RenderQueueBuffer(mRenderContext, pPreRenderBuffer, 0);
                    pPreRenderBuffer = NULL;
                }
            }
            bFirstPictureShowed = 0;
            //CJNIContext::setIsSurfaceChange(false);
            ReleaseSurface();
            isInitSurface = 0;
            m_surface_relased = true;
            continue;
        }
        
        //********************************************
        //  initialize layer and notify video size.
        //********************************************
        if(bFirstPictureShowed == 0)
        {
            int size[4];
            //* We should compute the real width and height again if the offset is valid.
            //* Because the pPicture->nWidth is not the real width, it is buffer widht.
            if((pPicture->nBottomOffset != 0 || pPicture->nRightOffset != 0) &&
                pPicture->nRightOffset <= pPicture->nLineStride)
            {
                size[0] = pPicture->nRightOffset - pPicture->nLeftOffset;
                size[1] = pPicture->nBottomOffset - pPicture->nTopOffset;
                size[2] = 0;
                size[3] = 0;
            }
            else
            {
                size[0] = pPicture->nWidth;
                size[1] = pPicture->nHeight;
                size[2] = 0;
                size[3] = 0;
            }
            
            // TODO: notify ui the size 

            if((pPicture->nRightOffset - pPicture->nLeftOffset) > 0 && 
               (pPicture->nBottomOffset - pPicture->nTopOffset) > 0)
            {
              size[0] = pPicture->nLeftOffset;
              size[1] = pPicture->nTopOffset;
              size[2] = pPicture->nRightOffset - pPicture->nLeftOffset;
              size[3] = pPicture->nBottomOffset - pPicture->nTopOffset;
              // TODO: notify crop
            }
    	
            if(mRenderContext != NULL)
            {
                m_render_dll->RenderSetExpectPixelFormat(mRenderContext, (enum EPIXELFORMAT)pPicture->ePixelFormat);

                //* if use deinterlace, decise by if DeinterlaceCreate() success
                CLog::Log(LOGDEBUG, "picture is progressive = %d ", pPicture->bIsProgressive);
                if (/*m_render_dll->RenderDeinterlaceValid() &&*/
                    pPicture->bIsProgressive == 0 &&
                    USE_DETNTERLACE == 1)
                {
                    if(pDeinterlacePrePicture != NULL)
                    {
                        m_dll->ReturnPicture(aw_private->pVideoCodec, pDeinterlacePrePicture);
                        pDeinterlacePrePicture = NULL;
                    }

                    if (m_render_dll->RenderDeinterlaceInit(mRenderContext) == 0)
                    {
                        int di_flag = m_render_dll->RenderDeinterlaceGetFlag(mRenderContext);
                        bDeinterlaceFlag      = 1;
                        nDeinterlaceDispNum   = (di_flag == 1/*DE_INTERLACE_HW*/) ? 2 : 1;
                        /*
                        if (MemAdapterGetDramFreq() < 360000 && pPicture->nHeight >= 1080) {
                            nDeinterlaceDispNum = 1;
                        } else if (di_flag == DE_INTERLACE_HW) {
                            nDeinterlaceDispNum = 2;
                        } else {
                            nDeinterlaceDispNum = 1;
                        }
                        */
                        CLog::Log(LOGDEBUG, " deinterlace , di_flag[%d], nDeinterlaceDispNum[%d]", di_flag, nDeinterlaceDispNum);
                        m_render_dll->RenderSetRenderToHardwareFlag(mRenderContext, 0);
                        m_render_dll->RenderSetExpectPixelFormat(mRenderContext, (EPIXELFORMAT)m_render_dll->RenderDeinterlaceGetExpectPixelFormat(mRenderContext));
                        m_render_dll->RenderSetDeinterlaceFlag(mRenderContext, di_flag);
                    }
                    else
                    {
                        nDeinterlaceDispNum = 1;
                        CLog::Log(LOGDEBUG, " open deinterlace failed , we not to use deinterlace!");
                    }
                }
                else
                {
                    nDeinterlaceDispNum = 1;
                }
                m_render_dll->RenderSetPictureSize(mRenderContext, pPicture->nWidth, pPicture->nHeight);
                m_render_dll->RenderSetDisplayRegion(mRenderContext,
                                                                            pPicture->nLeftOffset,
                                                                            pPicture->nTopOffset,
                                                                            pPicture->nRightOffset - pPicture->nLeftOffset,
                                                                            pPicture->nBottomOffset - pPicture->nTopOffset);
            }
        }

        if (bFirstPictureShowed == 1 && bDeinterlaceFlag == 1)
        {
            // for low display frame rate (24HZ), deinterlace operation will cause video always slow then audio
            // we drop the frame if need
            sleepTime = pPicture->nPts - GetPlayerPts();
            sleepTime = sleepTime/1000;          
            if (sleepTime < -70) {
                CLog::Log(LOGDEBUG, "deinterlace frame delay(%lld), drop it", sleepTime);
                if(pPicture != pDeinterlacePrePicture 
                    && pDeinterlacePrePicture != NULL)
                {
                    m_dll->ReturnPicture(aw_private->pVideoCodec, pDeinterlacePrePicture);
                }
                pDeinterlacePrePicture = pPicture;
                pPicture = NULL;
                continue;
            }
        }

        if (mRenderContext != NULL) {
            //CLog::Log(LOGINFO, "Process: nDeinterlaceDispNum = %d,bDeinterlaceFlag = %d",nDeinterlaceDispNum,bDeinterlaceFlag);
            for (int nDeinterlaceTime = 0;nDeinterlaceTime < nDeinterlaceDispNum; nDeinterlaceTime++) 
            {
                //CLog::Log(LOGDEBUG, "nDeinterlaceTime = %d", nDeinterlaceTime);
                // i) acquire buffer for deinterlace.
                //    buffer from a) dequeue buffer
                //                b) last non-queue buffer(pPreLayerBuffer).
                if (pPreRenderBuffer != NULL)
                {
                    pRenderBuffer    = pPreRenderBuffer;
                    pPreRenderBuffer = NULL;
                }
                else
                    nRenderBufferMode = m_render_dll->RenderDequeueBuffer(mRenderContext, &pRenderBuffer);

                if (nRenderBufferMode == 0x02) 
                {
                    // use outsize buffer
                    pRenderBuffer = pPicture;
                    pPicture = NULL;
                } 
                else if (nRenderBufferMode == 0) 
                {
                    if (bDeinterlaceFlag == 1) 
                    {
                        if (pDeinterlacePrePicture == NULL) 
                            pDeinterlacePrePicture = pPicture;

                        // ii) deinterlace process
                        int diret = m_render_dll->RenderDeinterlacProcess(mRenderContext, 
                                                                  pDeinterlacePrePicture, 
                                                                  pPicture, 
                                                                  pRenderBuffer, 
                                                                  nDeinterlaceTime);
                        //CLog::Log(LOGDEBUG, "deinterlace ret = %d ",  diret);
                        if (diret != 0) 
                        {
                            CLog::Log(LOGDEBUG, "Process : deinterlace process fail");
                            m_render_dll->RenderDeinterlaceReset(mRenderContext);
                            // iii) deinterlace error handling.
                            //      two ways for handling pLayerBuffer,
                            //          a) cancel to layer
                            //          b) set to pPreLayerBuffer for next deinterlace process.
                            //      we use b).
                            if (nDeinterlaceTime == (nDeinterlaceDispNum - 1)) {
                                // last fail, quit render msg.                        
                                if(pPicture != pDeinterlacePrePicture)
                                m_dll->ReturnPicture(aw_private->pVideoCodec, pDeinterlacePrePicture);
                                pDeinterlacePrePicture = pPicture;
                                pPreRenderBuffer = pRenderBuffer;
                                pRenderBuffer = NULL;
                                goto __RENDER_START;
                            } 
                            else {
                                // first fail, try deinterlace last field.
                                // prepicture == curpicture
                                if (pPicture != pDeinterlacePrePicture)
                                    m_dll->ReturnPicture(aw_private->pVideoCodec, pDeinterlacePrePicture);
                                pDeinterlacePrePicture = pPicture;
                                pPreRenderBuffer = pRenderBuffer;
                                pRenderBuffer = NULL;
                                nDeinterlaceTime ++;                        
                                continue;
                            }
                        }

                        // iv) end of deinterlacing this frame, set prepicture for next frame.
                        if(nDeinterlaceTime == (nDeinterlaceDispNum - 1))
                        {
                            if (pPicture != pDeinterlacePrePicture 
                                && pDeinterlacePrePicture != NULL)
                            {
                                m_dll->ReturnPicture(aw_private->pVideoCodec, pDeinterlacePrePicture);
                            }
                            pDeinterlacePrePicture = pPicture;
                            pPicture = NULL;
                        } 
                    }
                    else 
                    {
                        // copy picture only
                        m_dll->RotatePicture(pPicture, 
                                  pRenderBuffer, 
                                  0,    /*rotation*/
                                  16,  /*nGpuYAlign*/
                                  16); /*nGpuCAlign*/
                        m_dll->ReturnPicture(aw_private->pVideoCodec, pPicture);
                        pPicture = NULL;
                    }
                }
                else 
                {
                    CLog::Log(LOGERROR, "Process : dequeue buffer fail!!!!!");
                }

                
                if (bFirstPictureShowed == 1 && pRenderBuffer) 
                {
                    sleepTime = pRenderBuffer->nPts - GetPlayerPts();
                    if (sleepTime < 0)
                        CLog::Log(LOGDEBUG, "Process : sleepTime = %lld, Pts=%lld, dispts=%lld", sleepTime, pRenderBuffer->nPts, GetPlayerPts());

                    if (sleepTime > 5000000)
                    {
                        CLog::Log(LOGDEBUG, "Process : picture too far,maybe seek, drop pictrue");
                        if (bDeinterlaceFlag == 0) {
                            m_dll->ReturnPicture(aw_private->pVideoCodec, pRenderBuffer);
                            pRenderBuffer = NULL;
                        } 
                        else if (bDeinterlaceFlag == 1) 
                        {
                            pPreRenderBuffer = pRenderBuffer;
                            pRenderBuffer = NULL;
                            if (nDeinterlaceTime == 0)
                            {
                                if(pPicture != pDeinterlacePrePicture 
                                    && pDeinterlacePrePicture != NULL)
                                {
                                    m_dll->ReturnPicture(aw_private->pVideoCodec, pDeinterlacePrePicture);
                                }
                                pDeinterlacePrePicture = pPicture;
                                pPicture = NULL;
                            }
                        }
                        break; // next request picture
                    }
                    else if (sleepTime > 0)
                    {
                        if (sleepTime > 120000)
                            usleep(sleepTime-90000);
                        tempTime = ptsToAbs(pRenderBuffer->nPts, GetPlayerPts());
                        if (tempTime > getSystemTime()){
                            m_render_dll->RenderSetBufferTimeStamp(mRenderContext, tempTime*1000);
                        }
                        else{
                            CLog::Log(LOGDEBUG, "Process : picture delay, render now");
                        }
                        dropCount = 0;
                    }
                    else {
                        //sleepTime = pRenderBuffer->nPts - GetPlayerPts();
                        sleepTime = sleepTime/1000;
                        int dispFPS = m_render_dll->GetDispFps(mRenderContext);
                        int frameRate = pRenderBuffer->nFrameRate;
                        int dropTime = -1500;
                        if (frameRate == 0)
                            frameRate = 25;
                        if (frameRate > 1000)
                            frameRate = (frameRate + 999) / 1000;
                        if (frameRate > dispFPS)
                            dropTime = -100;
                        
                        //CLog::Log(LOGDEBUG, "sleepTime[%lld], fps[%d], framerate[%d], dropTime[%d]", sleepTime, dispFPS, frameRate, dropTime);
                        if ((bDeinterlaceFlag == 1) && ((sleepTime < -100) || (dropCount >= 30))) 
                        {
                            CLog::Log(LOGDEBUG, "Process : deinterlace(%d) too delay(%lld), drop pictrue", nDeinterlaceTime, sleepTime);
                            pPreRenderBuffer = pRenderBuffer;
                            pRenderBuffer    = NULL;
                            // always drop
                            if (nDeinterlaceTime == 0)
                            {
                                if(pPicture != pDeinterlacePrePicture 
                                && pDeinterlacePrePicture != NULL)
                                {
                                    m_dll->ReturnPicture(aw_private->pVideoCodec, pDeinterlacePrePicture);
                                }
                                pDeinterlacePrePicture = pPicture;
                                pPicture = NULL;
                            } 
                            dropCount = 0;
                            break;
                        }
                        else if ((bDeinterlaceFlag == 0) &&((sleepTime < dropTime) || (dropCount >= 30))) {  
                            CLog::Log(LOGDEBUG, "Process : picture too delay, drop pictrue(%d)", dropCount);
                            if (nRenderBufferMode == 0x02) {
                                m_dll->ReturnPicture(aw_private->pVideoCodec, pRenderBuffer); 
                                pRenderBuffer = NULL;
                            }    
                            pPreRenderBuffer = pRenderBuffer;                            
                            pRenderBuffer = NULL;
                            pPicture = NULL;
                            dropCount = 0;
                            break;						
                        }
                        dropCount++;
                        //CLog::Log(LOGDEBUG, "Process : picture delay, render now");
                    }
                }

                if (m_bStop)
                    break;

                if (pRenderBuffer != NULL)
                {
                    //CLog::Log(LOGDEBUG, "RenderPicture start dispFps=%d,video fps=%d",m_render_dll->GetDispFps(mRenderContext), pRenderBuffer->nFrameRate);
                    m_render_dll->RenderQueueBuffer(mRenderContext, pRenderBuffer, 1);
                    //CLog::Log(LOGDEBUG, "RenderPicture end");
                    pRenderBuffer = NULL;
                }
                
            }

            if (bFirstPictureShowed == 0)
                bFirstPictureShowed = 1;    
        }
    }
    //* return buffers before quit.
    if (pPicture != NULL) 
    {
        m_dll->ReturnPicture(aw_private->pVideoCodec, pPicture);
        pPicture = NULL;
    }
    if (pDeinterlacePrePicture != NULL)
    {
        m_dll->ReturnPicture(aw_private->pVideoCodec,  pDeinterlacePrePicture);
        pDeinterlacePrePicture = NULL;
    }    
    if (mRenderContext != NULL) {
        if (pRenderBuffer != NULL)
        {
            m_render_dll->RenderQueueBuffer(mRenderContext, pRenderBuffer, 0);
            pRenderBuffer = NULL;
        }    
        if(pPreRenderBuffer != NULL)
        {
            m_render_dll->RenderQueueBuffer(mRenderContext, pPreRenderBuffer, 0);
            pPreRenderBuffer = NULL;
        }
    }  
    SetPriority(THREAD_PRIORITY_NORMAL);
    CLog::Log(LOGDEBUG, "CAWCodec::Process Stopped");
}

void CAWCodec::SetSurface(void *jniEnv,  jni::jhobject *surface)
{
  CLog::Log(LOGDEBUG, "CAWCodec::SetSurface");
  mRenderContext = m_render_dll->RenderInit(jniEnv,  surface->get());
  if (mRenderContext != NULL) {
	m_render_dll->SetCallback(mRenderContext, CAWCodec::RendCallBack, (void*)this);
  }
}

void CAWCodec::ReleaseSurface() 
{
    CLog::Log(LOGDEBUG, "CAWCodec::ReleaseSurface");
    if (mRenderContext != NULL) {
        m_render_dll->RenderDeinit(mRenderContext);
        mRenderContext = NULL;
    }
}

int64_t CAWCodec::GetPlayerPts()
{
  int64_t clock_pts = 0;
  CDVDClock *playerclock = CDVDClock::GetMasterClock();
  if (playerclock)
    clock_pts = playerclock->GetClock();

  return clock_pts;
}

void CAWCodec::ResetSurface() 
{
  CLog::Log(LOGDEBUG, "CAWCodec::ResetSurface");
  jni::jhobject surface = CJNIContext::getSurface();
  if (surface.get() != 0)
  {
      ReleaseSurface();
      SetSurface((void*)xbmc_jnienv(), &surface);
  }
}

void CAWCodec::SetAppState(int state) 
{
    CLog::Log(LOGDEBUG, "CAWCodec::SetAppState (%d)", state);
    m_appState = state;
    if (!m_opened)
        return;
    
    if (state == 0) 
    {
        // release render context first, it use video codec to return picture
        m_surface_relased = false;
        int count = 0;
        while(!m_surface_relased && count++ < 60) {
            Wait(m_surface_relased, 50);
        }

        //next release video decoder 
        /*
        if (aw_private->pVideoCodec != NULL) 
        {
            m_dll->DestroyVideoDecoder(aw_private->pVideoCodec);
            aw_private->pVideoCodec = NULL;
        }
        */
    }      
}

void CAWCodec::SetStreamInputFormat(const char *name) 
{
    if (name != NULL)
        CLog::Log(LOGDEBUG, "CAWCodec::SetStreamInputFormat (%s)", name);
    
    m_streamInputFormat = (char *)name;
}