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

#include <math.h>

#include "DVDVideoCodecAW.h"
#include "DVDClock.h"
#include "DVDStreamInfo.h"
#include "AWCodec.h"
#include "utils/BitstreamConverter.h"
#include "utils/log.h"

#if defined(TARGET_ANDROID)
#include "android/activity/AndroidFeatures.h"
#include "android/jni/Context.h"
#include "android/jni/SurfaceView.h"
#include "android/jni/CreateSurfaceView.h"
#endif

#define __MODULE_NAME__ "CDVDVideoCodecAW"

typedef struct frame_queue {
  double dts;
  double pts;
  double sort_time;
  struct frame_queue *nextframe;
} frame_queue;

CDVDVideoCodecAW::CDVDVideoCodecAW() :
  m_Codec(NULL),
  m_pFormatName("awcodec"),
  m_last_pts(0.0),
  m_frame_queue(NULL),
  m_queue_depth(0),
  m_framerate(0.0),
  m_video_rate(0),
  m_mpeg2_sequence(NULL),
  m_bitparser(NULL),
  m_bitstream(NULL),
  m_streamInputFormat(NULL)
{
  m_surfaceHasCreated = false;
  pthread_mutex_init(&m_queue_mutex, NULL);
}

CDVDVideoCodecAW::~CDVDVideoCodecAW()
{
  CLog::Log(0,"CDVDVideoCodecAW::~CDVDVideoCodecAW");
  Dispose();
  pthread_mutex_destroy(&m_queue_mutex);
}

bool CDVDVideoCodecAW::Open(CDVDStreamInfo &hints, CDVDCodecOptions &options)
{
   if (hints.height <= 480) {
      CLog::Log(LOGDEBUG, "CDVDVideoCodecAW::Open height <= 480");
      return false;
   }
   
   m_hints = hints;
   CLog::Log(LOGDEBUG, "CDVDVideoCodecAW::Open --> hints.codec(%d)", m_hints.codec);
   switch(m_hints.codec)
   {
    case AV_CODEC_ID_MJPEG:
		m_pFormatName = "awcodec-mjpeg";
		break;
    case AV_CODEC_ID_MPEG1VIDEO:
		m_pFormatName = "awcodec-mjpeg1";
		break;
    case AV_CODEC_ID_MPEG2VIDEO:
		m_pFormatName = "awcodec-mjpeg2";
		break;
    case AV_CODEC_ID_MPEG2VIDEO_XVMC:
      if (m_hints.width <= 1280)
      {
        // amcodec struggles with VOB playback
        // which can be handled via software
        return false;
        break;
      }
      m_mpeg2_sequence_pts = 0;
      m_mpeg2_sequence = new mpeg2_sequence;
      m_mpeg2_sequence->width  = m_hints.width;
      m_mpeg2_sequence->height = m_hints.height;
      m_mpeg2_sequence->ratio  = m_hints.aspect;
      if (m_hints.rfpsrate > 0 && m_hints.rfpsscale != 0)
        m_mpeg2_sequence->rate = (float)m_hints.rfpsrate / m_hints.rfpsscale;
      else if (m_hints.fpsrate > 0 && m_hints.fpsscale != 0)
        m_mpeg2_sequence->rate = (float)m_hints.fpsrate / m_hints.fpsscale;
      else
        m_mpeg2_sequence->rate = 1.0;
      m_pFormatName = "awcodec-mpeg2";
      break;
	  
    case AV_CODEC_ID_H264:
      m_pFormatName = "awcodec-h264";
      // convert h264-avcC to h264-annex-b as h264-avcC
      // under streamers can have issues when seeking.
      if (m_hints.extradata)
      {
        m_bitstream = new CBitstreamConverter;
        if (!m_bitstream->Open(m_hints.codec, (uint8_t*)m_hints.extradata, m_hints.extrasize, true))
        {
          SAFE_DELETE(m_bitstream);
        } 
        else {
            // make sure we do not leak the existing m_hints.extradata
            free(m_hints.extradata);
            m_hints.extrasize = m_bitstream->GetExtraSize();
            m_hints.extradata = malloc(m_hints.extrasize);
            memcpy(m_hints.extradata, m_bitstream->GetExtraData(), m_hints.extrasize);
        }
      }
      //m_bitparser = new CBitstreamParser();
      //m_bitparser->Open();
      break;
    case AV_CODEC_ID_MPEG4:
    case AV_CODEC_ID_MSMPEG4V2:
    case AV_CODEC_ID_MSMPEG4V3:
      m_pFormatName = "awcodec-mpeg4";
      break;
    case AV_CODEC_ID_H263:
    case AV_CODEC_ID_H263P:
    case AV_CODEC_ID_H263I:
      m_pFormatName = "awcodec-h263";
      break;
    case AV_CODEC_ID_FLV1:
      m_pFormatName = "awcodec-flv1";
      break;
    case AV_CODEC_ID_RV10:
    case AV_CODEC_ID_RV20:
	m_pFormatName = "awcodec-rx";
	break;
    case AV_CODEC_ID_RV30:
    case AV_CODEC_ID_RV40:
      //m_pFormatName = "awcodec-rv";
      return false; // specific data not match aw codec ??
      break;
    case AV_CODEC_ID_VC1:
      m_pFormatName = "awcodec-vc1";
      break;
    case AV_CODEC_ID_WMV3:
      m_pFormatName = "awcodec-wmv3";
      break;
    case AV_CODEC_ID_AVS:
    case AV_CODEC_ID_CAVS:
      m_pFormatName = "awcodec-avs";
      break;
    case AV_CODEC_ID_HEVC:   
    m_pFormatName = "awcodec-h265";      
    // check for hevc-hvcC and convert to h265-annex-b      
    if (m_hints.extradata)      
    {        
        m_bitstream = new CBitstreamConverter;        
        if (!m_bitstream->Open(m_hints.codec, (uint8_t*)m_hints.extradata, m_hints.extrasize, true))       
        {          
            SAFE_DELETE(m_bitstream);        
        }
        else 
        {
            // make sure we do not leak the existing m_hints.extradata
            free(m_hints.extradata);
            m_hints.extrasize = m_bitstream->GetExtraSize();
            m_hints.extradata = malloc(m_hints.extrasize);
            memcpy(m_hints.extradata, m_bitstream->GetExtraData(), m_hints.extrasize);
        }
    }      
    break;  
    default:
      CLog::Log(LOGDEBUG, "%s: Unknown hints.codec(%d)", __MODULE_NAME__, m_hints.codec);
      return false;
      break;
  }

  m_aspect_ratio = m_hints.aspect;
  m_Codec = new CAWCodec();
  if (!m_Codec)
  {
    CLog::Log(LOGERROR, "%s: Failed to create AllwinnerTech Codec", __MODULE_NAME__);
    return false;
  }
  m_opened = false;

  // allocate a dummy DVDVideoPicture buffer.
  // first make sure all properties are reset.
  memset(&m_videobuffer, 0, sizeof(DVDVideoPicture));

  m_videobuffer.dts = DVD_NOPTS_VALUE;
  m_videobuffer.pts = DVD_NOPTS_VALUE;
  m_videobuffer.format = RENDER_FMT_BYPASS;
  m_videobuffer.color_range  = 0;
  m_videobuffer.color_matrix = 4;
  m_videobuffer.iFlags  = DVP_FLAG_ALLOCATED;
  m_videobuffer.iWidth  = m_hints.width;
  m_videobuffer.iHeight = m_hints.height;

  m_videobuffer.iDisplayWidth  = m_videobuffer.iWidth;
  m_videobuffer.iDisplayHeight = m_videobuffer.iHeight;
  if (m_hints.aspect > 0.0 && !m_hints.forced_aspect)
  {
    m_videobuffer.iDisplayWidth  = ((int)lrint(m_videobuffer.iHeight * m_hints.aspect)) & -3;
    if (m_videobuffer.iDisplayWidth > m_videobuffer.iWidth)
    {
      m_videobuffer.iDisplayWidth  = m_videobuffer.iWidth;
      m_videobuffer.iDisplayHeight = ((int)lrint(m_videobuffer.iWidth / m_hints.aspect)) & -3;
    }
  }

  //InitSurfaceTexture();
  //m_Codec->SetSurfaceView((void*)xbmc_jnienv(), *m_surfaceView);
  /*
  jni::jhobject surface = CJNIContext::getSurface();
  m_Codec->SetSurface((void*)xbmc_jnienv(), &surface);
  CJNIContext::setIsSurfaceChange(false);
  */
  if (!m_surfaceHasCreated && m_hints.width > 0 && m_hints.height > 0) {
      CLog::Log(LOGINFO, "%s: Create surface", __MODULE_NAME__);    
      CJNICreateSurfaceView *mainView = new CJNICreateSurfaceView();
      mainView->CreateSurface();
      m_surfaceHasCreated = true;      
  }

  CLog::Log(LOGINFO, "%s: Opened AllwinnerTech Codec", __MODULE_NAME__);    
  return true;
}

void CDVDVideoCodecAW::Dispose(void)
{
  if (m_Codec)
    m_Codec->CloseDecoder(), delete m_Codec, m_Codec = NULL;

  if (m_surfaceHasCreated) {
      CJNICreateSurfaceView *mainView = new CJNICreateSurfaceView();
      mainView->DeleteSurface();
      m_surfaceHasCreated = false;
  }

  while (m_queue_depth)
    FrameQueuePop();

  m_opened = false;
}

int CDVDVideoCodecAW::Decode(uint8_t *pData, int iSize, double dts, double pts)
{
  // Handle Input, add demuxer packet to input queue, we must accept it or
  // it will be discarded as DVDPlayerVideo has no concept of "try again".
  //CLog::Log(LOGDEBUG,"CDVDVideoCodecAW::Decode pData[%x] iSize[%d] dts[%ld] pts[%ld]",pData, iSize, dts, pts);
  if (!m_surfaceHasCreated) { 
      CLog::Log(LOGDBUS, "%s: HACK-> create surface in decode!!!", __MODULE_NAME__);
      CJNICreateSurfaceView *mainView = new CJNICreateSurfaceView();
      mainView->CreateSurface();
      m_surfaceHasCreated = true;    
  }
  
  if (pData)
  {
    if (m_bitstream)
    {
      if (!m_bitstream->Convert(pData, iSize))
        return VC_ERROR;

      pData = m_bitstream->GetConvertBuffer();
      iSize = m_bitstream->GetConvertSize();
    }

    if (m_bitparser)
      m_bitparser->FindIdrSlice(pData, iSize);

    FrameRateTracking( pData, iSize, dts, pts);
  }

  if (!m_opened)
  {
    if (m_Codec) m_Codec->SetStreamInputFormat(m_streamInputFormat);
    if (m_Codec && !m_Codec->OpenDecoder(m_hints)) 
    {
      CLog::Log(LOGERROR, "%s: Failed to open AllwinnerTech Codec", __MODULE_NAME__);
      return VC_ERROR;
    }
    m_opened = true;
  }

  if (m_hints.ptsinvalid)
    pts = DVD_NOPTS_VALUE;

  return m_Codec->Decode(pData, iSize, dts, pts);
}

void CDVDVideoCodecAW::Reset(void)
{
  while (m_queue_depth)
    FrameQueuePop();

  m_Codec->Reset();
  m_mpeg2_sequence_pts = 0;
}

bool CDVDVideoCodecAW::GetPicture(DVDVideoPicture* pDvdVideoPicture)
{
  if (m_Codec)
    m_Codec->GetPicture(&m_videobuffer);
  *pDvdVideoPicture = m_videobuffer;

  // check for mpeg2 aspect ratio changes
  if (m_mpeg2_sequence && pDvdVideoPicture->pts >= m_mpeg2_sequence_pts)
    m_aspect_ratio = m_mpeg2_sequence->ratio;

  pDvdVideoPicture->iDisplayWidth  = pDvdVideoPicture->iWidth;
  pDvdVideoPicture->iDisplayHeight = pDvdVideoPicture->iHeight;
  if (m_aspect_ratio > 1.0 && !m_hints.forced_aspect)
  {
    pDvdVideoPicture->iDisplayWidth  = ((int)lrint(pDvdVideoPicture->iHeight * m_aspect_ratio)) & -3;
    if (pDvdVideoPicture->iDisplayWidth > pDvdVideoPicture->iWidth)
    {
      pDvdVideoPicture->iDisplayWidth  = pDvdVideoPicture->iWidth;
      pDvdVideoPicture->iDisplayHeight = ((int)lrint(pDvdVideoPicture->iWidth / m_aspect_ratio)) & -3;
    }
  }

  return true;
}

void CDVDVideoCodecAW::SetDropState(bool bDrop)
{
}

void CDVDVideoCodecAW::SetSpeed(int iSpeed)
{
  if (m_Codec)
    m_Codec->SetSpeed(iSpeed);
}

int CDVDVideoCodecAW::GetDataSize(void)
{
  if (m_Codec)
    return m_Codec->GetDataSize();

  return 0;
}

double CDVDVideoCodecAW::GetTimeSize(void)
{
  if (m_Codec)
    return m_Codec->GetTimeSize();

  return 0.0;
}

void CDVDVideoCodecAW::FrameQueuePop(void)
{
  if (!m_frame_queue || m_queue_depth == 0)
    return;

  pthread_mutex_lock(&m_queue_mutex);
  // pop the top frame off the queue
  frame_queue *top = m_frame_queue;
  m_frame_queue = top->nextframe;
  m_queue_depth--;
  pthread_mutex_unlock(&m_queue_mutex);

  // and release it
  free(top);
}

void CDVDVideoCodecAW::FrameQueuePush(double dts, double pts)
{
  frame_queue *newframe = (frame_queue*)calloc(sizeof(frame_queue), 1);
  newframe->dts = dts;
  newframe->pts = pts;
  // if both dts or pts are good we use those, else use decoder insert time for frame sort
  if ((newframe->pts != DVD_NOPTS_VALUE) || (newframe->dts != DVD_NOPTS_VALUE))
  {
    // if pts is borked (stupid avi's), use dts for frame sort
    if (newframe->pts == DVD_NOPTS_VALUE)
      newframe->sort_time = newframe->dts;
    else
      newframe->sort_time = newframe->pts;
  }

  pthread_mutex_lock(&m_queue_mutex);
  frame_queue *queueWalker = m_frame_queue;
  if (!queueWalker || (newframe->sort_time < queueWalker->sort_time))
  {
    // we have an empty queue, or this frame earlier than the current queue head.
    newframe->nextframe = queueWalker;
    m_frame_queue = newframe;
  }
  else
  {
    // walk the queue and insert this frame where it belongs in display order.
    bool ptrInserted = false;
    frame_queue *nextframe = NULL;
    //
    while (!ptrInserted)
    {
      nextframe = queueWalker->nextframe;
      if (!nextframe || (newframe->sort_time < nextframe->sort_time))
      {
        // if the next frame is the tail of the queue, or our new frame is earlier.
        newframe->nextframe = nextframe;
        queueWalker->nextframe = newframe;
        ptrInserted = true;
      }
      queueWalker = nextframe;
    }
  }
  m_queue_depth++;
  pthread_mutex_unlock(&m_queue_mutex);	
}

void CDVDVideoCodecAW::FrameRateTracking(uint8_t *pData, int iSize, double dts, double pts)
{
  // mpeg2 handling
  if (m_mpeg2_sequence)
  {
    // probe demux for sequence_header_code NAL and
    // decode aspect ratio and frame rate.
    if (CBitstreamConverter::mpeg2_sequence_header(pData, iSize, m_mpeg2_sequence))
    {
      m_mpeg2_sequence_pts = pts;
      if (m_mpeg2_sequence_pts == DVD_NOPTS_VALUE)
        m_mpeg2_sequence_pts = dts;

      m_framerate = m_mpeg2_sequence->rate;
      m_video_rate = (int)(0.5 + (96000.0 / m_framerate));

      CLog::Log(LOGDEBUG, "%s: detected mpeg2 aspect ratio(%f), framerate(%f), video_rate(%d)",
        __MODULE_NAME__, m_mpeg2_sequence->ratio, m_framerate, m_video_rate);

      // update m_hints for 1st frame fixup.
      switch(m_mpeg2_sequence->rate_info)
      {
        default:
        case 0x01:
          m_hints.rfpsrate = 24000.0;
          m_hints.rfpsscale = 1001.0;
          break;
        case 0x02:
          m_hints.rfpsrate = 24000.0;
          m_hints.rfpsscale = 1000.0;
          break;
        case 0x03:
          m_hints.rfpsrate = 25000.0;
          m_hints.rfpsscale = 1000.0;
          break;
        case 0x04:
          m_hints.rfpsrate = 30000.0;
          m_hints.rfpsscale = 1001.0;
          break;
        case 0x05:
          m_hints.rfpsrate = 30000.0;
          m_hints.rfpsscale = 1000.0;
          break;
        case 0x06:
          m_hints.rfpsrate = 50000.0;
          m_hints.rfpsscale = 1000.0;
          break;
        case 0x07:
          m_hints.rfpsrate = 60000.0;
          m_hints.rfpsscale = 1001.0;
          break;
        case 0x08:
          m_hints.rfpsrate = 60000.0;
          m_hints.rfpsscale = 1000.0;
          break;
      }
      m_hints.width    = m_mpeg2_sequence->width;
      m_hints.height   = m_mpeg2_sequence->height;
      m_hints.aspect   = m_mpeg2_sequence->ratio;
      m_hints.fpsrate  = m_hints.rfpsrate;
      m_hints.fpsscale = m_hints.rfpsscale;
    }
    return;
  }

  // everything else
  FrameQueuePush(dts, pts);

  // we might have out-of-order pts,
  // so make sure we wait for at least 8 values in sorted queue.
  if (m_queue_depth > 16)
  {
    pthread_mutex_lock(&m_queue_mutex);

    float cur_pts = m_frame_queue->pts;
    if (cur_pts == DVD_NOPTS_VALUE)
      cur_pts = m_frame_queue->dts;

    pthread_mutex_unlock(&m_queue_mutex);	

    float duration = cur_pts - m_last_pts;
    m_last_pts = cur_pts;

    // clamp duration to sensible range,
    // 66 fsp to 20 fsp
    if (duration >= 15000.0 && duration <= 50000.0)
    {
      double framerate;
      switch((int)(0.5 + duration))
      {
        // 59.940 (16683.333333)
        case 16000 ... 17000:
          framerate = 60000.0 / 1001.0;
          break;

        // 50.000 (20000.000000)
        case 20000:
          framerate = 50000.0 / 1000.0;
          break;

        // 49.950 (20020.000000)
        case 20020:
          framerate = 50000.0 / 1001.0;
          break;

        // 29.970 (33366.666656)
        case 32000 ... 35000:
          framerate = 30000.0 / 1001.0;
          break;

        // 25.000 (40000.000000)
        case 40000:
          framerate = 25000.0 / 1000.0;
          break;

        // 24.975 (40040.000000)
        case 40040:
          framerate = 25000.0 / 1001.0;
          break;

        /*
        // 24.000 (41666.666666)
        case 41667:
          framerate = 24000.0 / 1000.0;
          break;
        */

        // 23.976 (41708.33333)
        case 40200 ... 43200:
          // 23.976 seems to have the crappiest encodings :)
          framerate = 24000.0 / 1001.0;
          break;

        default:
          framerate = 0.0;
          //CLog::Log(LOGDEBUG, "%s: unknown duration(%f), cur_pts(%f)",
          //  __MODULE_NAME__, duration, cur_pts);
          break;
      }

      if (framerate > 0.0 && (int)m_framerate != (int)framerate)
      {
        m_framerate = framerate;
        m_video_rate = (int)(0.5 + (96000.0 / framerate));
        CLog::Log(LOGDEBUG, "%s: detected new framerate(%f), video_rate(%d)",
          __MODULE_NAME__, m_framerate, m_video_rate);
      }
    }

    FrameQueuePop();
  }
}

void CDVDVideoCodecAW::InitSurfaceTexture(void)
{
  m_surfaceView = new CJNISurfaceView();
}
void CDVDVideoCodecAW::ReleaseSurfaceTexture(void)
{
   SAFE_DELETE(m_surfaceView);
}

void CDVDVideoCodecAW::SetAppState(int state)
{
     CLog::Log(LOGDEBUG, "CDVDVideoCodecAW::SetAppState(%d)", state);
     if (!m_opened)
        return;
     if (m_Codec)
        m_Codec->SetAppState(state);
}

void CDVDVideoCodecAW::SetStreamInputFormat(const char *name) 
{
    m_streamInputFormat = (char *)name;
}