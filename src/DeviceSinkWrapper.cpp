#include "DeviceSink.h"

DeviceSinkWrapper::DeviceSinkWrapper (std::string video, int format, int width, int height, int fps, std::string audio, int audioFreq, int audioNbChannels)
{
	if (!video.empty())
	{
		videoDevice=video;
		std::list<unsigned int> videoformatList;
		if (format) 
		{
			videoformatList.push_back(format);
		};
		if ((videoformatList.empty()) && (format!=0)) {
			videoformatList.push_back(V4L2_PIX_FMT_H264);
			videoformatList.push_back(V4L2_PIX_FMT_MJPEG);
			videoformatList.push_back(V4L2_PIX_FMT_JPEG);
		}
		// Init video capture
		V4L2DeviceParameters param(video.c_str(), videoformatList, width, height, fps, 0, openFlags);
		videoSource = V4l2Capture::create(param, ioTypeIn);
	}
#ifdef HAVE_ALSA	
	if (!audio.empty())
	{
		std::list<snd_pcm_format_t> audioFmtList;
		if (audioFmt != SND_PCM_FORMAT_UNKNOWN)
		{
			audioFmtList.push_back(audioFmt);
		}
		// default audio format tries
		if (audioFmtList.empty()) {
			audioFmtList.push_back(SND_PCM_FORMAT_S16_LE);
			audioFmtList.push_back(SND_PCM_FORMAT_S16_BE);
		}
		// find the ALSA device associated with the V4L2 device
		audioDevice = getV4l2Alsa(audio);
	
		// Init audio capture
		ALSACaptureParameters param(audioDevice.c_str(), audioFmtList, audioFreq, audioNbChannels, 0);
		audioSource = ALSACapture::createNew(param);
	}		
#endif	
}

#ifdef HAVE_ALSA	
std::string  DeviceSinkWrapper::getV4l2Alsa(const std::string& v4l2device) {
	std::string audioDevice(v4l2device);
	
	std::map<std::string,std::string> videodevices;
	std::string video4linuxPath("/sys/class/video4linux");
	DIR *dp = opendir(video4linuxPath.c_str());
	if (dp != NULL) {
		struct dirent *entry = NULL;
		while((entry = readdir(dp))) {
			std::string devicename;
			std::string deviceid;
			if (strstr(entry->d_name,"video") == entry->d_name) {
				std::string ueventPath(video4linuxPath);
				ueventPath.append("/").append(entry->d_name).append("/device/uevent");
				std::ifstream ifsd(ueventPath.c_str());
				deviceid = std::string(std::istreambuf_iterator<char>{ifsd}, {});
				deviceid.erase(deviceid.find_last_not_of("\n")+1);
			}

			if (!deviceid.empty()) {
				videodevices[entry->d_name] = getDeviceId(deviceid);
			}
		}
		closedir(dp);
	}

	std::map<std::string,std::string> audiodevices;
	int rcard = -1;
	while ( (snd_card_next(&rcard) == 0) && (rcard>=0) ) {
		void **hints = NULL;
		if (snd_device_name_hint(rcard, "pcm", &hints) >= 0) {
			void **str = hints;
			while (*str) {				
				std::ostringstream os;
				os << "/sys/class/sound/card" << rcard << "/device/uevent";

				std::ifstream ifs(os.str().c_str());
				std::string deviceid = std::string(std::istreambuf_iterator<char>{ifs}, {});
				deviceid.erase(deviceid.find_last_not_of("\n")+1);
				deviceid = getDeviceId(deviceid);

				if (!deviceid.empty()) {
					if (audiodevices.find(deviceid) == audiodevices.end()) {
						std::string audioname = snd_device_name_get_hint(*str, "NAME");
						audiodevices[deviceid] = audioname;
					}
				}

				str++;
			}

			snd_device_name_free_hint(hints);
		}
	}

	auto deviceId  = videodevices.find(getDeviceName(v4l2device));
	if (deviceId != videodevices.end()) {
		auto audioDeviceIt = audiodevices.find(deviceId->second);
		
		if (audioDeviceIt != audiodevices.end()) {
			audioDevice = audioDeviceIt->second;
			std::cout <<  v4l2device << "=>" << audioDevice << std::endl;			
		}
	}
	
	
	return audioDevice;
}

std::string DeviceSinkWrapper::getAudioDevice() {
	return audioDevice;
}
#endif	

std::string DeviceSinkWrapper::getVideoDevice() {
	return videoDevice;
}

std::string DeviceSinkWrapper::getDeviceName(const std::string & devicePath)
{
	std::string deviceName(devicePath);
	size_t pos = deviceName.find_last_of('/');
	if (pos != std::string::npos) {
		deviceName.erase(0,pos+1);
	}
	return deviceName;
}

std::string DeviceSinkWrapper::getDeviceId(const std::string& evt) {
    std::string deviceid;
    std::istringstream f(evt);
    std::string key;
    while (getline(f, key, '=')) {
            std::string value;
	    if (getline(f, value)) {
		    if ( (key =="PRODUCT") || (key == "PCI_SUBSYS_ID") ) {
			    deviceid = value;
			    break;
		    }
	    }
    }
    return deviceid;
}

void DeviceSinkWrapper::setFramedSource(FramedSource *source) {
	if ( deviceSource == NULL ) {
		deviceSource = (V4L2DeviceSource *)source;
	}
}
int DeviceSinkWrapper::openOutput(std::string outputFile) {
	if (!outputFile.empty() && deviceSource)
	{
		V4L2DeviceParameters outparam(outputFile.c_str(), videoSource->getFormat(), videoSource->getWidth(), videoSource->getHeight(), 0, 0);
		fileSink = V4l2Output::create(outparam, ioTypeOut);
		if (fileSink != NULL)
		{
			outFd = fileSink->getFd();
			deviceSource -> setOutputFd(outFd);
		}
	}
}

int DeviceSinkWrapper::getOutputFd() {
	return outFd;
}

void DeviceSinkWrapper::closeOutput() {
	if (fileSink != NULL)
	{
		delete fileSink;
		fileSink = NULL;
		outFd = -1;
	}
}
void DeviceSinkWrapper::setMulticastUrl(std::string url) {
	multicastUrl = url;
}
void DeviceSinkWrapper::setUnicastUrl(std::string url) {
	unicastUrl = url;
}
void DeviceSinkWrapper::setHlsUrl(std::string url) {
	hlsUrl = url;
}
V4l2Capture* DeviceSinkWrapper::getVideoSource() {
	return videoSource;
}
#ifdef HAVE_ALSA
AlsaCapture* DeviceSinkWrapper::getAudioSource() {
	return audioSource;
}
#endif
std::string DeviceSinkWrapper::getMulticastUrl() {
	return multicastUrl;
}
std::string DeviceSinkWrapper::getUnicastUrl() {
	return unicastUrl;
}
void DeviceSinkWrapper::setIoTypeIn(V4l2Access::IoType iotype) {
	ioTypeIn = iotype;
}
void DeviceSinkWrapper::setIoTypeOut(V4l2Access::IoType iotype) {
	ioTypeOut = iotype;
}
void DeviceSinkWrapper::setOpenFlags(int open_flags) {
	openFlags = open_flags;
}
