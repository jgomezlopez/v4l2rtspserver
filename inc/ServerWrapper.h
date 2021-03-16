#include "DeviceSink.h"

#ifndef __RTSP_SERVER_WRAPPER__
#define __RTSP_SERVER_WRAPPER__
class RtspServerWrapper {
	public:
		RtspServerWrapper();
		void setRtspPort(unsigned short port);
		void setRtspOverHTTPPort(unsigned short port);
		void setTimeout(int time_out);
		void setHlsSegment(int segment);
		void setRealm(const char* pass_realm);
		void setWebroot(std::string web_root);
		void setRtpBasePort(unsigned short port);
		void setMulticastAddress(std::string maddr);
		void setRepeatConfig(bool repeat_config);
		void addUser(std::string user);
		void attachDevice(DeviceSinkWrapper &device);
		void init();
		void start();
		void stop();
		~RtspServerWrapper();

	private:
		int nbSource = 0;
		char quit = 0;
		pthread_t thread_stream;
		struct in_addr destinationAddress;
		static void *run(void *obj);
		unsigned short currRtpPort;
		unsigned short baseRtpPort;
		V4l2RTSPServer *rtspServer = NULL;
		unsigned short rtspPort;
		unsigned short rtspOverHTTPPort;
		int timeout;
		int hlsSegment;
		bool repeatConfig = true;
		const char* realm;
		std::string webroot;
		std::string multicastAddress;
		std::list<std::string> userPasswordList;
		void decodeMulticastUrl();
};
#endif
