/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2018, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     Analyze.h
    \brief    encoder analyzer class (header)
*/

#ifndef __ANALYZE__
#define __ANALYZE__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <stdio.h>
#include <memory.h>
#include <assert.h>
#include "CommonLib/CommonDef.h"
#include "CommonLib/ChromaFormat.h"
#include "math.h"
#if EXTENSION_360_VIDEO
#include "AppEncHelper360/TExt360EncAnalyze.h"
#endif

//! \ingroup EncoderLib
//! \{

// ====================================================================================================================
// Class definition
// ====================================================================================================================

#if ENABLE_QPA
 #define FRAME_WEIGHTING 0 // WPSNR temporal weighting according to hierarchical coding structure; only for GOP size 16
#endif

/// encoder analyzer class
class Analyze
{
private:
  double    m_dPSNRSum[MAX_NUM_COMPONENT];
  double    m_dAddBits;
  uint32_t      m_uiNumPic;
  double    m_dFrmRate; //--CFG_KDY
  double    m_MSEyuvframe[MAX_NUM_COMPONENT]; // sum of MSEs
#if ENABLE_QPA && FRAME_WEIGHTING
  double    m_sumWSSD[MAX_NUM_COMPONENT];   // weighted SSDs
  double    m_sumW;
#endif
#if EXTENSION_360_VIDEO
  TExt360EncAnalyze m_ext360;
#endif

public:
  virtual ~Analyze()  {}
  Analyze() { clear(); }

  void  addResult( double psnr[MAX_NUM_COMPONENT], double bits, const double MSEyuvframe[MAX_NUM_COMPONENT])
  {
    m_dAddBits  += bits;
    for(uint32_t i=0; i<MAX_NUM_COMPONENT; i++)
    {
      m_dPSNRSum[i] += psnr[i];
      m_MSEyuvframe[i] += MSEyuvframe[i];
    }

    m_uiNumPic++;
  }
#if ENABLE_QPA
 #if FRAME_WEIGHTING
  void    addWeightedSSD(const double dWeightedSSD, const ComponentID compID) { m_sumWSSD[compID] += dWeightedSSD; }
  void    addWeight     (const double dWeight) { m_sumW += dWeight; }
  double  getWPSNR      (const ComponentID compID) const { return (m_sumWSSD[compID] > 0.0 ? 10.0 * log10(m_sumW / m_sumWSSD[compID]) : 999.99); }
 #else
  double  getWPSNR      (const ComponentID compID) const { return m_dPSNRSum[compID] / (double)m_uiNumPic; }
 #endif
#endif
  double  getPsnr(ComponentID compID) const { return  m_dPSNRSum[compID];  }
  double  getBits()                   const { return  m_dAddBits;   }
  void    setBits(double numBits)     { m_dAddBits = numBits; }
  uint32_t    getNumPic()                 const { return  m_uiNumPic;   }
#if EXTENSION_360_VIDEO
  TExt360EncAnalyze& getExt360Info() { return m_ext360; }
#endif

  void    setFrmRate  (double dFrameRate) { m_dFrmRate = dFrameRate; } //--CFG_KDY
  void    clear()
  {
    m_dAddBits = 0;
    for(uint32_t i=0; i<MAX_NUM_COMPONENT; i++)
    {
      m_dPSNRSum[i] = 0;
      m_MSEyuvframe[i] = 0;
#if ENABLE_QPA && FRAME_WEIGHTING
      m_sumWSSD[i] = 0;
#endif
    }
#if ENABLE_QPA && FRAME_WEIGHTING
    m_sumW = 0;
#endif
    m_uiNumPic = 0;
#if EXTENSION_360_VIDEO
    m_ext360.clear();
#endif
  }


  void calculateCombinedValues(const ChromaFormat chFmt, double &PSNRyuv, double &MSEyuv, const BitDepths &bitDepths)
  {
    MSEyuv    = 0;
    int scale = 0;

    int maximumBitDepth = bitDepths.recon[CHANNEL_TYPE_LUMA];
    for (uint32_t channelTypeIndex = 1; channelTypeIndex < MAX_NUM_CHANNEL_TYPE; channelTypeIndex++)
    {
      if (bitDepths.recon[channelTypeIndex] > maximumBitDepth)
      {
        maximumBitDepth = bitDepths.recon[channelTypeIndex];
      }
    }

#if ENABLE_QPA
    const uint32_t maxval                = /*useWPSNR ? (1 << maximumBitDepth) - 1 :*/ 255 << (maximumBitDepth - 8); // fix with WPSNR: 1023 (4095) instead of 1020 (4080) for bit depth 10 (12)
#else
    const uint32_t maxval                = 255 << (maximumBitDepth - 8);
#endif
    const uint32_t numberValidComponents = getNumberValidComponents(chFmt);

    for (uint32_t comp=0; comp<numberValidComponents; comp++)
    {
      const ComponentID compID        = ComponentID(comp);
      const uint32_t        csx           = getComponentScaleX(compID, chFmt);
      const uint32_t        csy           = getComponentScaleY(compID, chFmt);
      const int         scaleChan     = (4>>(csx+csy));
      const uint32_t        bitDepthShift = 2 * (maximumBitDepth - bitDepths.recon[toChannelType(compID)]); //*2 because this is a squared number

      const double      channelMSE    = (m_MSEyuvframe[compID] * double(1 << bitDepthShift)) / double(getNumPic());

      scale  += scaleChan;
      MSEyuv += scaleChan * channelMSE;
    }

    MSEyuv /= double(scale);  // i.e. divide by 6 for 4:2:0, 8 for 4:2:2 etc.
    PSNRyuv = (MSEyuv == 0) ? 999.99 : 10.0 * log10((maxval * maxval) / MSEyuv);
  }


#if ENABLE_QPA || WCG_WPSNR
  void    printOut ( char cDelim, const ChromaFormat chFmt, const bool printMSEBasedSNR, const bool printSequenceMSE, const BitDepths &bitDepths, const bool useWPSNR = false )
#else
  void    printOut ( char cDelim, const ChromaFormat chFmt, const bool printMSEBasedSNR, const bool printSequenceMSE, const BitDepths &bitDepths )
#endif
  {
#if !WCG_WPSNR
    MsgLevel e_msg_level = cDelim == 'a' ? INFO: DETAILS;
#else
    MsgLevel e_msg_level = (cDelim == 'a') || (cDelim == 'w') ? INFO : DETAILS;
#endif
    double dFps     =   m_dFrmRate; //--CFG_KDY
    double dScale   = dFps / 1000 / (double)m_uiNumPic;

    double MSEBasedSNR[MAX_NUM_COMPONENT];
    if (printMSEBasedSNR)
    {
      for (uint32_t componentIndex = 0; componentIndex < MAX_NUM_COMPONENT; componentIndex++)
      {
        const ComponentID compID = ComponentID(componentIndex);

        if (getNumPic() == 0)
        {
          MSEBasedSNR[compID] = 0 * dScale; // this is the same calculation that will be evaluated for any other statistic when there are no frames (it should result in NaN). We use it here so all the output is consistent.
        }
        else
        {
#if ENABLE_QPA
          const uint32_t maxval = /*useWPSNR ? (1 << bitDepths.recon[toChannelType(compID)]) - 1 :*/ 255 << (bitDepths.recon[toChannelType(compID)] - 8); // fix with WPSNR: 1023 (4095) instead of 1020 (4080) for bit depth 10 (12)
#else
          //NOTE: this is not the true maximum value for any bitDepth other than 8. It comes from the original HM PSNR calculation
          const uint32_t maxval = 255 << (bitDepths.recon[toChannelType(compID)] - 8);
#endif
          const double MSE  = m_MSEyuvframe[compID];

          MSEBasedSNR[compID] = (MSE == 0) ? 999.99 : 10.0 * log10((maxval * maxval) / (MSE / (double)getNumPic()));
        }
      }
    }

    switch (chFmt)
    {
      case CHROMA_400:
        if (printMSEBasedSNR)
        {
#if ENABLE_QPA || WCG_WPSNR
          if (useWPSNR) {
            msg( e_msg_level, "         \tTotal Frames |   "   "Bitrate     "  "Y-WPSNR" );
          } else
#endif
          msg( e_msg_level, "         \tTotal Frames |   "   "Bitrate     "  "Y-PSNR" );

          if (printSequenceMSE)
          {
            msg( e_msg_level, "    Y-MSE\n" );
          }
          else
          {
            msg( e_msg_level, "\n");
          }

          //msg( e_msg_level, "\t------------ "  " ----------"   " -------- "  " -------- "  " --------\n" );
          msg( e_msg_level, "Average: \t %8d    %c "          "%12.4lf  "    "%8.4lf",
                 getNumPic(), cDelim,
                 getBits() * dScale,
#if ENABLE_QPA
                 useWPSNR ? getWPSNR(COMPONENT_Y) :
#endif
                 getPsnr(COMPONENT_Y) / (double)getNumPic() );

          if (printSequenceMSE)
          {
            msg( e_msg_level, "  %8.4lf\n", m_MSEyuvframe[COMPONENT_Y] / (double)getNumPic() );
          }
          else
          {
            msg( e_msg_level, "\n");
          }

          msg( e_msg_level, "From MSE:\t %8d    %c "          "%12.4lf  "    "%8.4lf\n",
                 getNumPic(), cDelim,
                 getBits() * dScale,
                 MSEBasedSNR[COMPONENT_Y] );
        }
        else
        {
#if ENABLE_QPA || WCG_WPSNR
          if (useWPSNR) {
            msg( e_msg_level, "\tTotal Frames |   "   "Bitrate     "  "Y-WPSNR" );
          } else
#endif
          msg( e_msg_level, "\tTotal Frames |   "   "Bitrate     "  "Y-PSNR" );

          if (printSequenceMSE)
          {
            msg( e_msg_level, "    Y-MSE\n" );
          }
          else
          {
            msg( e_msg_level, "\n");
          }

          //msg( e_msg_level, "\t------------ "  " ----------"   " -------- "  " -------- "  " --------\n" );
          msg( e_msg_level, "\t %8d    %c "          "%12.4lf  "    "%8.4lf",
                 getNumPic(), cDelim,
                 getBits() * dScale,
#if ENABLE_QPA
                 useWPSNR ? getWPSNR(COMPONENT_Y) :
#endif
                 getPsnr(COMPONENT_Y) / (double)getNumPic() );

          if (printSequenceMSE)
          {
            msg( e_msg_level, "  %8.4lf\n", m_MSEyuvframe[COMPONENT_Y] / (double)getNumPic() );
          }
          else
          {
            msg( e_msg_level, "\n");
          }
        }
        break;
      case CHROMA_420:
      case CHROMA_422:
      case CHROMA_444:
        {
          double PSNRyuv = MAX_DOUBLE;
          double MSEyuv  = MAX_DOUBLE;

          calculateCombinedValues(chFmt, PSNRyuv, MSEyuv, bitDepths);

          if (printMSEBasedSNR)
          {
#if ENABLE_QPA || WCG_WPSNR
            if (useWPSNR) {
              msg( e_msg_level, "         \tTotal Frames |   "   "Bitrate     "  "Y-WPSNR   "  "U-WPSNR   "  "V-WPSNR   "  "YUV-WPSNR" );
            } else
#endif
            msg( e_msg_level, "         \tTotal Frames |   "   "Bitrate     "  "Y-PSNR    "  "U-PSNR    "  "V-PSNR    "  "YUV-PSNR " );

            if (printSequenceMSE)
            {
              msg( e_msg_level, " Y-MSE     "  "U-MSE     "  "V-MSE    "  "YUV-MSE \n" );
            }
            else
            {
              msg( e_msg_level, "\n");
            }

            //msg( e_msg_level, "\t------------ "  " ----------"   " -------- "  " -------- "  " --------\n" );
            msg( e_msg_level, "Average: \t %8d    %c "          "%12.4lf  "    "%8.4lf  "   "%8.4lf  "    "%8.4lf  "   "%8.4lf",
                   getNumPic(), cDelim,
                   getBits() * dScale,
#if ENABLE_QPA
                   useWPSNR ? getWPSNR(COMPONENT_Y ) :
#endif
                   getPsnr(COMPONENT_Y ) / (double)getNumPic(),
#if ENABLE_QPA
                   useWPSNR ? getWPSNR(COMPONENT_Cb) :
#endif
                   getPsnr(COMPONENT_Cb) / (double)getNumPic(),
#if ENABLE_QPA
                   useWPSNR ? getWPSNR(COMPONENT_Cr) :
#endif
                   getPsnr(COMPONENT_Cr) / (double)getNumPic(),
                   PSNRyuv );

            if (printSequenceMSE)
            {
              msg( e_msg_level, "  %8.4lf  "   "%8.4lf  "    "%8.4lf  "   "%8.4lf\n",
                     m_MSEyuvframe[COMPONENT_Y ] / (double)getNumPic(),
                     m_MSEyuvframe[COMPONENT_Cb] / (double)getNumPic(),
                     m_MSEyuvframe[COMPONENT_Cr] / (double)getNumPic(),
                     MSEyuv );
            }
            else
            {
              msg( e_msg_level, "\n");
            }

            msg( e_msg_level, "From MSE:\t %8d    %c "          "%12.4lf  "    "%8.4lf  "   "%8.4lf  "    "%8.4lf  "   "%8.4lf\n",
                   getNumPic(), cDelim,
                   getBits() * dScale,
                   MSEBasedSNR[COMPONENT_Y ],
                   MSEBasedSNR[COMPONENT_Cb],
                   MSEBasedSNR[COMPONENT_Cr],
                   PSNRyuv );
          }
          else
          {
#if ENABLE_QPA || WCG_WPSNR
            if (useWPSNR) {
              msg( e_msg_level, "\tTotal Frames |   "   "Bitrate     "  "Y-WPSNR   "  "U-WPSNR   "  "V-WPSNR   "  "YUV-WPSNR" );
            } else
#endif
            msg( e_msg_level, "\tTotal Frames |   "   "Bitrate     "  "Y-PSNR    "  "U-PSNR    "  "V-PSNR    "  "YUV-PSNR " );
#if EXTENSION_360_VIDEO
            m_ext360.printHeader(e_msg_level);
#endif

            if (printSequenceMSE)
            {
              msg( e_msg_level, " Y-MSE     "  "U-MSE     "  "V-MSE    "  "YUV-MSE \n" );
            }
            else
            {
              msg( e_msg_level, "\n");
            }

            //msg( e_msg_level, "\t------------ "  " ----------"   " -------- "  " -------- "  " --------\n" );
            msg( e_msg_level, "\t %8d    %c "          "%12.4lf  "    "%8.4lf  "   "%8.4lf  "    "%8.4lf  "   "%8.4lf",
                   getNumPic(), cDelim,
                   getBits() * dScale,
#if ENABLE_QPA
                   useWPSNR ? getWPSNR(COMPONENT_Y ) :
#endif
                   getPsnr(COMPONENT_Y ) / (double)getNumPic(),
#if ENABLE_QPA
                   useWPSNR ? getWPSNR(COMPONENT_Cb) :
#endif
                   getPsnr(COMPONENT_Cb) / (double)getNumPic(),
#if ENABLE_QPA
                   useWPSNR ? getWPSNR(COMPONENT_Cr) :
#endif
                   getPsnr(COMPONENT_Cr) / (double)getNumPic(),
                   PSNRyuv );

#if EXTENSION_360_VIDEO
            m_ext360.printPSNRs(getNumPic(), e_msg_level);
#endif

            if (printSequenceMSE)
            {
              msg( e_msg_level, "  %8.4lf  "   "%8.4lf  "    "%8.4lf  "   "%8.4lf\n",
                     m_MSEyuvframe[COMPONENT_Y ] / (double)getNumPic(),
                     m_MSEyuvframe[COMPONENT_Cb] / (double)getNumPic(),
                     m_MSEyuvframe[COMPONENT_Cr] / (double)getNumPic(),
                     MSEyuv );
            }
            else
            {
              msg( e_msg_level, "\n");
            }
          }
        }
        break;
      default:
        msg( ERROR, "Unknown format during print out\n");
        exit(1);
        break;
    }
  }


  void    printSummary(const ChromaFormat chFmt, const bool printSequenceMSE, const BitDepths &bitDepths, const std::string &sFilename)
  {
    FILE* pFile = fopen (sFilename.c_str(), "at");

    double dFps     =   m_dFrmRate; //--CFG_KDY
    double dScale   = dFps / 1000 / (double)m_uiNumPic;
    switch (chFmt)
    {
      case CHROMA_400:
        fprintf(pFile, "%f\t %f\n",
            getBits() * dScale,
            getPsnr(COMPONENT_Y) / (double)getNumPic() );
        break;
      case CHROMA_420:
      case CHROMA_422:
      case CHROMA_444:
        {
          double PSNRyuv = MAX_DOUBLE;
          double MSEyuv  = MAX_DOUBLE;

          calculateCombinedValues(chFmt, PSNRyuv, MSEyuv, bitDepths);

          fprintf(pFile, "%f\t %f\t %f\t %f\t %f",
              getBits() * dScale,
              getPsnr(COMPONENT_Y ) / (double)getNumPic(),
              getPsnr(COMPONENT_Cb) / (double)getNumPic(),
              getPsnr(COMPONENT_Cr) / (double)getNumPic(),
              PSNRyuv );

          if (printSequenceMSE)
          {
            fprintf(pFile, "\t %f\t %f\t %f\t %f\n",
                m_MSEyuvframe[COMPONENT_Y ] / (double)getNumPic(),
                m_MSEyuvframe[COMPONENT_Cb] / (double)getNumPic(),
                m_MSEyuvframe[COMPONENT_Cr] / (double)getNumPic(),
                MSEyuv );
          }
          else
          {
            fprintf(pFile, "\n");
          }

          break;
        }

      default:
          msg( ERROR, "Unknown format during print out\n");
          exit(1);
          break;
    }

    fclose(pFile);
  }
};

extern Analyze             m_gcAnalyzeAll;
extern Analyze             m_gcAnalyzeI;
extern Analyze             m_gcAnalyzeP;
extern Analyze             m_gcAnalyzeB;
#if WCG_WPSNR
extern Analyze             m_gcAnalyzeWPSNR;
#endif
extern Analyze             m_gcAnalyzeAll_in;

//! \}

#endif // !defined(AFX_TENCANALYZE_H__C79BCAA2_6AC8_4175_A0FE_CF02F5829233__INCLUDED_)
