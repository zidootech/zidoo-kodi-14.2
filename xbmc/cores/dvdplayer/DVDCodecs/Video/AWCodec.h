#pragma once
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

#include "DVDVideoCodec.h"
#include "cores/dvdplayer/DVDStreamInfo.h"
#include "cores/VideoRenderers/RenderFeatures.h"
#include "guilib/Geometry.h"
#include "rendering/RenderSystem.h"
#include "threads/Thread.h"
#include "PlatformDefs.h"

#include "android/jni/JNIBase.h"

class DllLibAwCodec;
class DllLibAwRender;
class CJNISurfaceView;

//Add AW
typedef void* VideoDecoder;
typedef int (*Callback)(void* pUserData, int eMessageId, void* param);
typedef struct aw_private_t aw_private_t;
//typedef enum VideoCodecFormat_t VideoCodecFormat_t;
//typedef struct VideoPicture VideoPicture;

typedef struct VIDEOPICTURE
{
    int     nID;
    int     nStreamIndex;
    int     ePixelFormat;
    int     nWidth;
    int     nHeight;
    int     nLineStride;
    int     nTopOffset;
    int     nLeftOffset;
    int     nBottomOffset;
    int     nRightOffset;
    int     nFrameRate;
    int     nAspectRatio;
    int     bIsProgressive;
    int     bTopFieldFirst;
    int     bRepeatTopField;
    int64_t nPts;
    int64_t nPcr;
    char*   pData0;
    char*   pData1;
    char*   pData2;
    char*   pData3;
    int     bMafValid;
    char*   pMafData;
    int     nMafFlagStride;
    int     bPreFrmValid;
    int     nBufId;
    unsigned int phyYBufAddr;
    unsigned int phyCBufAddr;
    void*   pPrivate;
}VideoPicture;

typedef void* RenderContext;
typedef int (*PlayerCallback)(void *pUserData, VideoPicture* picture);

typedef struct VIDEOSTREAMINFO
{
    int   eCodecFormat;
    int   nWidth;
    int   nHeight;
    int   nFrameRate;
    int   nFrameDuration;
    int   nAspectRatio;
    int   bIs3DStream;
    int   nCodecSpecificDataLen;
    char* pCodecSpecificData;
    int   bSecureStreamFlag;
    int   bSecureStreamFlagLevel1;
	int   bIsFramePackage; /* 1: frame package;  0: stream package */
	int   h265ReferencePictureNum;
}VideoStreamInfo;

typedef struct VCONFIG
{
    int bScaleDownEn;
    int bRotationEn;
    int nHorizonScaleDownRatio;
    int nVerticalScaleDownRatio;
    int nSecHorizonScaleDownRatio;
    int nSecVerticalScaleDownRatio;
    int nRotateDegree;
    int bThumbnailMode;
    int eOutputPixelFormat;
    int bNoBFrames;
    int bDisable3D;
    int bSupportMaf;
    int bDispErrorFrame;
    int nVbvBufferSize;
    int bSecureosEn;
	//Callback  callback;
    void*     pUserData;
    int  bGpuBufValid;
    int  nAlignStride;
    int  bIsSoftDecoderFlag;
}VConfig;

typedef struct VIDEOSTREAMDATAINFO
{
    char*   pData;
    int     nLength;
    int64_t nPts;
    int64_t nPcr;
    int     bIsFirstPart;
    int     bIsLastPart;
    int     nID;
    int     nStreamIndex;
    int     bValid;
    unsigned int     bVideoInfoFlag;
    void*   pVideoInfo;
}VideoStreamDataInfo;



typedef struct AwPacketT
{
    void*			buf;
    void*			ringBuf;
    int				buflen;
    int				ringBufLen;
    long long		pts;   
    long long		duration;
    int				type;
    int 			length;
    unsigned int	flags; /* MINOR_STREAM, FIRST_PART, LAST_PART, etc... */
    int				streamIndex;
    long long		pcr;
    int 			infoVersion;
    void*			info;//VideoInfo/AudioInfo/SubtitleInfo
}AwPacket;




/*
typedef struct aw_packet 
{
    //AVPacket      avpkt;
    int64_t       avpts;
    int64_t       avdts;
    int           avduration;
    int           isvalid;
    int           newflag;
    int64_t       lastpts;
    unsigned char *data;
    unsigned char *buf;
    int           data_size;
    int           buf_size;
    //hdr_buf_t     *hdr;
    //codec_para_t  *codec;
} CdxPacketT;
*/

typedef enum EVIDEOCODECFORMAT
{
    VIDEO_CODEC_FORMAT_UNKNOWN          = 0,
    VIDEO_CODEC_FORMAT_MJPEG            = 0x101,
    VIDEO_CODEC_FORMAT_MPEG1            = 0x102,
    VIDEO_CODEC_FORMAT_MPEG2            = 0x103,
    VIDEO_CODEC_FORMAT_MPEG4            = 0x104,
    VIDEO_CODEC_FORMAT_MSMPEG4V1        = 0x105,
    VIDEO_CODEC_FORMAT_MSMPEG4V2        = 0x106,
    VIDEO_CODEC_FORMAT_DIVX3            = 0x107,
    VIDEO_CODEC_FORMAT_DIVX4            = 0x108,
    VIDEO_CODEC_FORMAT_DIVX5            = 0x109,
    VIDEO_CODEC_FORMAT_XVID             = 0x10a,
    VIDEO_CODEC_FORMAT_H263             = 0x10b,
    VIDEO_CODEC_FORMAT_SORENSSON_H263   = 0x10c,
    VIDEO_CODEC_FORMAT_RXG2             = 0x10d,
    VIDEO_CODEC_FORMAT_WMV1             = 0x10e,
    VIDEO_CODEC_FORMAT_WMV2             = 0x10f,
    VIDEO_CODEC_FORMAT_WMV3             = 0x110,
    VIDEO_CODEC_FORMAT_VP6              = 0x111,
    VIDEO_CODEC_FORMAT_VP8              = 0x112,
    VIDEO_CODEC_FORMAT_VP9              = 0x113,
    VIDEO_CODEC_FORMAT_RX               = 0x114,
    VIDEO_CODEC_FORMAT_H264             = 0x115,
    VIDEO_CODEC_FORMAT_H265             = 0x116,
    VIDEO_CODEC_FORMAT_AVS              = 0x117,
    
    VIDEO_CODEC_FORMAT_MAX              = VIDEO_CODEC_FORMAT_AVS,
    VIDEO_CODEC_FORMAT_MIN              = VIDEO_CODEC_FORMAT_MJPEG,
}VideoCodecFormat_t;

typedef enum EPIXELFORMAT
{
    PIXEL_FORMAT_DEFAULT            = 0,
    
    PIXEL_FORMAT_YUV_PLANER_420     = 1,
    PIXEL_FORMAT_YUV_PLANER_422     = 2,
    PIXEL_FORMAT_YUV_PLANER_444     = 3,
    
    PIXEL_FORMAT_YV12               = 4,
    PIXEL_FORMAT_NV21               = 5,
    PIXEL_FORMAT_NV12               = 6,
    PIXEL_FORMAT_YUV_MB32_420       = 7,
    PIXEL_FORMAT_YUV_MB32_422       = 8,
    PIXEL_FORMAT_YUV_MB32_444       = 9,
    
    PIXEL_FORMAT_MIN = PIXEL_FORMAT_DEFAULT,
    PIXEL_FORMAT_MAX = PIXEL_FORMAT_YUV_MB32_444,
}EPixelFormat_t;

enum EVDECODERESULT
{
    VDECODE_RESULT_UNSUPPORTED       = -1,
    VDECODE_RESULT_OK                = 0,
    VDECODE_RESULT_FRAME_DECODED     = 1,
    VDECODE_RESULT_CONTINUE          = 2,
    VDECODE_RESULT_KEYFRAME_DECODED  = 3,
    VDECODE_RESULT_NO_FRAME_BUFFER   = 4,
    VDECODE_RESULT_NO_BITSTREAM      = 5,
    VDECODE_RESULT_RESOLUTION_CHANGE = 6,
    
    VDECODE_RESULT_MIN = VDECODE_RESULT_UNSUPPORTED,
    VDECODE_RESULT_MAX = VDECODE_RESULT_RESOLUTION_CHANGE,
};


typedef struct aw_private_t
{
  DllLibAwCodec     	*m_dll;
  VideoDecoder			*pVideoCodec;
  AwPacket				awPacket;
  VideoStreamDataInfo	dataInfo;
  VideoPicture* 		pPicture;
  int					nDecodeFrameCount;
  FILE 					*fp_es;
  VideoCodecFormat_t 	vCodecFormat;
  EPixelFormat_t		PixelFormat;

} aw_private_t;


class CAWCodec : public CThread
{
public:
  CAWCodec();
  virtual ~CAWCodec();

  bool          OpenDecoder(CDVDStreamInfo &hints);
  void          CloseDecoder();
  void          Reset();

  int           Decode(uint8_t *pData, size_t size, double dts, double pts);

  bool          GetPicture(DVDVideoPicture* pDvdVideoPicture);
  void          SetSpeed(int speed);
  int           GetDataSize();
  double        GetTimeSize();
  /*
  void           SetSurfaceView(void *jniEnv, CJNISurfaceView &surfaceView);
  void           ReleaseSurfaceView();
  */
  void           SetSurface(void *jniEnv, jni::jhobject  *surface);
  void           ReleaseSurface();  
  int 		GetUndisplayNum();
  int64_t       GetPlayerPts();
  int Wait(volatile bool& bStop, int timeout);
  static int  RendCallBack(void* pUserData, VideoPicture* picture);  
  void ResetSurface();
  void SetAppState(int state);
  void SetStreamInputFormat(const char *name);
  char *m_streamInputFormat;
   
protected:
	virtual 	void  	Process();   
    
private:

  DllLibAwCodec   *m_dll;
  DllLibAwRender *m_render_dll;
  bool             m_opened;
  aw_private_t    *aw_private;
  CDVDStreamInfo   m_hints;
  volatile int64_t m_1st_pts;
  volatile int64_t m_cur_pts;
  volatile int64_t m_cur_pictcnt;
  volatile int64_t m_old_pictcnt;
  volatile double  m_timesize;
  volatile int64_t m_vbufsize;
  int64_t          m_start_dts;
  int64_t          m_start_pts;
  CEvent           m_ready_event;
  CCriticalSection m_presentlock;
  XbmcThreads::ConditionVariable  m_presentevent;  

  CRect            m_dst_rect;
  int              m_view_mode;
  RENDER_STEREO_MODE m_stereo_mode;
  RENDER_STEREO_VIEW m_stereo_view;
  float            m_zoom;
  int              m_contrast;
  int              m_brightness;

	void	ConfigOutputFormat();
	int 	GetOutputPicture(VideoPicture *pict, int yBufferSize, int cBufferSize);
	void	 GetYUVData(VideoPicture *pict, FILE *fp);
	int 				m_src_offset[4];
	int 				m_src_stride[4];

	RenderContext *mRenderContext;

	int m_speed;
    VideoStreamInfo  m_streamInfo;
    int bConfigDropDelayFrames;
    bool m_do_reset;
    bool m_surface_relased; // when app pause, use this to wait surface release
    int m_appState; // 0 pause, 1 unpause
};

