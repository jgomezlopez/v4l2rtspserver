#include <dirent.h>
#include <sstream>
#include "V4l2Device.h"
#include "V4l2Capture.h"
#include "V4l2Output.h"

#include "liveMedia.hh"
#include "DeviceSourceFactory.h"
#include "V4l2RTSPServer.h"

#ifdef HAVE_ALSA
#include "ALSACapture.h"
#endif

#ifndef __DEVICE_SINK_WRAPPER__
#define __DEVICE_SINK_WRAPPER__
class DeviceSinkWrapper {
	public:
		DeviceSinkWrapper(std::string video, int format, int width, int height, int fps, std::string audio, int audioFreq, int audioNbChannels);
		void setMulticastUrl(std::string url);
		void setUnicastUrl(std::string url);
		void setFramedSource(FramedSource *source);
		void setIoTypeIn(V4l2Access::IoType iotype);
		void setIoTypeOut(V4l2Access::IoType iotype);
		void setOpenFlags(int open_flags);
		int openOutput(std::string outputFile);
		void closeOutput();
		V4l2Capture* getVideoSource();
		std::string getVideoDevice();
#ifdef HAVE_ALSA
		AlsaCapture* getAudioSource();
		std::string getAudioDevice();
#endif
		std::string getMulticastUrl();
		std::string getUnicastUrl();
		int getOutputFd();
		void setHlsUrl(std::string url);
		std::string getHlsUrl();
		static std::string getDeviceName(const std::string & devicePath);
		
	private:
		V4l2Access::IoType ioTypeIn  = V4l2Access::IOTYPE_MMAP;
		V4l2Access::IoType ioTypeOut = V4l2Access::IOTYPE_MMAP;
		int openFlags = O_RDWR | O_NONBLOCK; 
		std::string videoDevice = "";
		std::string audioDevice = "";
		V4l2Capture* videoSource = NULL;
		V4l2Output* fileSink = NULL;
		int outFd = -1;
#ifdef HAVE_ALSA
		AlsaCapture* audioSource = NULL;
#endif
		std::string multicastUrl = "";
		std::string unicastUrl = "";
		std::string hlsUrl = "";
#ifdef HAVE_ALSA	
		static std::string getV4l2Alsa(const std::string& v4l2device);
#endif
		static std::string getDeviceId(const std::string& evt);
		V4L2DeviceSource *deviceSource=NULL;
};
#endif
