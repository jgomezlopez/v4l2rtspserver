/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** main.cpp
** 
** V4L2 RTSP streamer                                                                 
**                                                                                    
** H264 capture using V4L2                                                            
** RTSP using live555                                                                 
**                                                                                    
** -------------------------------------------------------------------------*/

#include <errno.h>
#include <sys/ioctl.h>
#include <dirent.h>

#include <sstream>

// libv4l2
#include <linux/videodev2.h>

// project
#include "logger.h"


#include "ServerWrapper.h"

// -------------------------------------------------------
//    decode multicast url <group>:<rtp_port>:<rtcp_port>
// -------------------------------------------------------
void RtspServerWrapper::decodeMulticastUrl()
{
	std::istringstream is(multicastAddress);
	std::string ip;
	getline(is, ip, ':');						
	if (!ip.empty())
	{
		destinationAddress.s_addr = inet_addr(ip.c_str());
	}						
	
	std::string port;
	getline(is, port, ':');						
	baseRtpPort = 20000;
	if (!port.empty())
	{
		baseRtpPort = atoi(port.c_str());
	}	
}

RtspServerWrapper::RtspServerWrapper()
{
	currRtpPort = 20000;
	baseRtpPort = 20000;
	rtspServer = NULL;
	rtspPort = 20000;
	rtspOverHTTPPort = 20000;
	timeout = 65;
	hlsSegment = 2;
	realm = NULL;
	webroot = "";
}

void RtspServerWrapper::addUser(std::string user){
	userPasswordList.push_back(user);
}

void RtspServerWrapper::init() {
	// create RTSP server
	if (rtspServer == NULL) {
		rtspServer = new V4l2RTSPServer(rtspPort, rtspOverHTTPPort, timeout, hlsSegment, userPasswordList, realm, webroot);
		if (!rtspServer->available()) 
		{
			LOG(ERROR) << "Failed to create RTSP server: " << rtspServer->getResultMsg();
			delete rtspServer;
			rtspServer = NULL;
		}
	}
	else {
		// decode multicast info
		destinationAddress.s_addr = chooseRandomIPv4SSMAddress(*rtspServer->env());
		decodeMulticastUrl();	
		currRtpPort = baseRtpPort;
	}
}

void RtspServerWrapper::attachDevice(DeviceSinkWrapper &device) {
	bool useThread = true;
	int ttl = 5;
	int queueSize = 10;
	if (rtspServer != NULL) {
	{		
		StreamReplicator* videoReplicator = NULL;
		std::string rtpVideoFormat;
		V4l2Capture* videoCapture = device.getVideoSource(); 
		if (videoCapture)
		{
			rtpVideoFormat.assign(V4l2RTSPServer::getVideoRtpFormat(videoCapture->getFormat()));
			if (rtpVideoFormat.empty()) {
				LOG(FATAL) << "No Streaming format supported for device " << device.getVideoDevice();
				delete videoCapture;
				
			} else {
				LOG(NOTICE) << "Create Source ..." << device.getVideoDevice();
				videoReplicator = DeviceSourceFactory::createStreamReplicator(rtspServer->env(), videoCapture->getFormat(), new DeviceCaptureAccess<V4l2Capture>(videoCapture), queueSize, useThread, device.getOutputFd(), repeatConfig);
				if (videoReplicator == NULL) 
				{
					LOG(FATAL) << "Unable to create source for device " << device.getVideoDevice();
					delete videoCapture;
				}
				else {
					device.setFramedSource(videoReplicator->inputSource());
				}
			}
		}
				
		// Init Audio Capture
		StreamReplicator* audioReplicator = NULL;
		std::string rtpAudioFormat;
#ifdef HAVE_ALSA
		ALSACapture* audioCapture = device.getAudioSource();
		if (audioCapture) 
		{
			rtpAudioFormat.assign(V4l2RTSPServer::getAudioRtpFormat(audioCapture->getFormat(),audioCapture->getSampleRate(), audioCapture->getChannels()));

			audioReplicator = DeviceSourceFactory::createStreamReplicator(rtspServer->env(), 0, new DeviceCaptureAccess<ALSACapture>(audioCapture), queueSize, useThread);
			if (audioReplicator == NULL) 
			{
				LOG(FATAL) << "Unable to create source for device " << device.getAudioDevice();
				delete audioCapture;
			}
		}
#endif
			// Create Multicast Session
			if (!multicastAddress.empty())						
			{		
			
				std::list<ServerMediaSubsession*> subSession;						
				if (videoReplicator)
				{
					LOG(NOTICE) << "RTP  address " << inet_ntoa(destinationAddress) << ":" << currRtpPort;
					subSession.push_back(MulticastServerMediaSubsession::createNew(*rtspServer->env(), destinationAddress, Port(currRtpPort), Port(currRtpPort+1), ttl, videoReplicator, rtpVideoFormat));					
					// increment ports for next sessions
					currRtpPort+=2;
				}
				
				if (audioReplicator)
				{
					LOG(NOTICE) << "RTCP address " << inet_ntoa(destinationAddress) << ":" << currRtpPort;
					subSession.push_back(MulticastServerMediaSubsession::createNew(*rtspServer->env(), destinationAddress, Port(currRtpPort), Port(currRtpPort+1), ttl, audioReplicator, rtpAudioFormat));				
					
					// increment ports for next sessions
					currRtpPort+=2;
				}
				nbSource += rtspServer->addSession(device.getMulticastUrl(), subSession);																
			}
			
			// Create HLS Session					
			if (hlsSegment > 0)
			{
				std::list<ServerMediaSubsession*> subSession;
				if (videoReplicator)
				{
					subSession.push_back(TSServerMediaSubsession::createNew(*rtspServer->env(), videoReplicator, rtpVideoFormat, audioReplicator, rtpAudioFormat, hlsSegment));				
				}
				nbSource += rtspServer->addSession(device.getUnicastUrl(), subSession);
				
				struct in_addr ip;
#if LIVEMEDIA_LIBRARY_VERSION_INT	<	1611878400				
				ip.s_addr = ourIPAddress(*rtspServer->env());
#else
				ip.s_addr = ourIPv4Address(*rtspServer->env());
#endif
				//LOG(NOTICE) << "HLS       http://" << inet_ntoa(ip) << ":" << rtspPort << "/" << baseUrl+tsurl << ".m3u8";
				//LOG(NOTICE) << "MPEG-DASH http://" << inet_ntoa(ip) << ":" << rtspPort << "/" << baseUrl+tsurl << ".mpd";
			}
			
			// Create Unicast Session					
			std::list<ServerMediaSubsession*> subSession;
			if (videoReplicator)
			{
				subSession.push_back(UnicastServerMediaSubsession::createNew(*rtspServer->env(), videoReplicator, rtpVideoFormat));				
			}
			if (audioReplicator)
			{
				subSession.push_back(UnicastServerMediaSubsession::createNew(*rtspServer->env(), audioReplicator, rtpAudioFormat));				
			}
			nbSource += rtspServer->addSession(device.getUnicastUrl(), subSession);				
		}
	}
}					

void *RtspServerWrapper::run(void *obj) {
	RtspServerWrapper *rtsp = (RtspServerWrapper *)obj;
	rtsp->rtspServer->eventLoop(&(rtsp->quit)); 
	return obj;
}

void RtspServerWrapper::start() {
	if (nbSource>0)
	{
		quit=0;
		pthread_create(&thread_stream, NULL, *run, (void *)this);	
	}
}

void RtspServerWrapper::stop() {
	quit=1;
}

void RtspServerWrapper::setRtspPort(unsigned short port) {
	rtspPort = port;
}
void RtspServerWrapper::setRtspOverHTTPPort(unsigned short port) {
	rtspOverHTTPPort = port;
}
void RtspServerWrapper::setTimeout(int time_out) {
	timeout = time_out;
}
void RtspServerWrapper::setHlsSegment(int segment) {
	hlsSegment = segment;
}
void RtspServerWrapper::setRealm(const char* pass_realm) {
	realm = pass_realm;
}
void RtspServerWrapper::setWebroot(std::string web_root) {
	webroot = web_root;
}
void RtspServerWrapper::setRtpBasePort(unsigned short port) {
	baseRtpPort = port;
}
void RtspServerWrapper::setMulticastAddress(std::string maddr) {
	multicastAddress = maddr;
}
void RtspServerWrapper::setRepeatConfig(bool repeat_config) {
	repeatConfig = repeat_config;
}
RtspServerWrapper::~RtspServerWrapper() {
	delete rtspServer;
}



