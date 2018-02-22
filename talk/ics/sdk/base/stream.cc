/*
 * Intel License
 */

#include "webrtc/rtc_base/helpers.h"
#include "webrtc/media/base/videocapturer.h"
#include "webrtc/media/engine/webrtcvideocapturerfactory.h"
#include "webrtc/modules/video_capture/video_capture_factory.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "talk/ics/sdk/base/customizedframescapturer.h"
#include "talk/ics/sdk/base/desktopcapturer.h"
#include "talk/ics/sdk/base/mediaconstraintsimpl.h"
#if defined(WEBRTC_IOS)
#include "talk/ics/sdk/base/objc/ObjcVideoCapturerInterface.h"
#endif
#include "talk/ics/sdk/base/peerconnectiondependencyfactory.h"
#include "talk/ics/sdk/base/webrtcvideorendererimpl.h"
#if defined(WEBRTC_WIN)
#include "talk/ics/sdk/base/win/videorendererwin.h"
#endif
#include "talk/ics/sdk/include/cpp/ics/base/deviceutils.h"
#include "talk/ics/sdk/include/cpp/ics/base/framegeneratorinterface.h"
#include "talk/ics/sdk/include/cpp/ics/base/stream.h"

namespace ics {
namespace base {

#if defined(WEBRTC_WIN)
Stream::Stream()
    : media_stream_(nullptr),
      renderer_impl_(nullptr),
      d3d9_renderer_impl_(nullptr),
      ended_(false),
      id_(""),
      source_(AudioSourceInfo::kUnknown, VideoSourceInfo::kUnknown) {}

Stream::Stream(const std::string& id)
    : media_stream_(nullptr),
      renderer_impl_(nullptr),
      d3d9_renderer_impl_(nullptr),
      ended_(false),
      id_(id),
      source_(AudioSourceInfo::kUnknown, VideoSourceInfo::kUnknown) {}
#else
Stream::Stream() : media_stream_(nullptr), renderer_impl_(nullptr), ended_(false), id_("") {}

Stream::Stream(MediaStreamInterface* media_stream, StreamSourceInfo source)
    : media_stream_(media_stream), source_(source) {}

Stream::Stream(const std::string& id)
    : media_stream_(nullptr), renderer_impl_(nullptr), ended_(false), id_(id) {}
#endif

MediaStreamInterface* Stream::MediaStream() const {
  return media_stream_;
}

Stream::~Stream() {
  DetachVideoRenderer();

  if (media_stream_)
    media_stream_->Release();
}

void Stream::MediaStream(MediaStreamInterface* media_stream) {
  if (media_stream == nullptr) {
    RTC_DCHECK(false);
    return;
  }

  if (media_stream_ != nullptr) {
    media_stream_->Release();
  }

  media_stream_ = media_stream;
  media_stream_->AddRef();
}

std::string Stream::Id() const {
  return id_;
}

void Stream::Id(const std::string& id) {
  id_ = id;
}

void Stream::DisableVideo() {
  SetVideoTracksEnabled(false);
}

void Stream::EnableVideo() {
  SetVideoTracksEnabled(true);
}

void Stream::DisableAudio() {
  SetAudioTracksEnabled(false);
}

void Stream::EnableAudio() {
  SetAudioTracksEnabled(true);
}

void Stream::SetVideoTracksEnabled(bool enabled) {
  if (media_stream_ == nullptr)
    return;
  auto video_tracks = media_stream_->GetVideoTracks();
  for (auto it = video_tracks.begin(); it != video_tracks.end(); ++it) {
    (*it)->set_enabled(enabled);
  }
}

void Stream::SetAudioTracksEnabled(bool enabled) {
  if (media_stream_ == nullptr)
    return;
  auto audio_tracks = media_stream_->GetAudioTracks();
  for (auto it = audio_tracks.begin(); it != audio_tracks.end(); ++it) {
    (*it)->set_enabled(enabled);
  }
}

void Stream::AttachVideoRenderer(VideoRendererARGBInterface& renderer){
  if (media_stream_ == nullptr) {
    LOG(LS_ERROR) << "Cannot attach an audio only stream to a renderer.";
    return;
  }

  auto video_tracks = media_stream_->GetVideoTracks();
  if (video_tracks.size() == 0) {
    LOG(LS_ERROR) << "Attach failed because of no video tracks.";
    return;
  } else if (video_tracks.size() > 1) {
    LOG(LS_WARNING) << "There are more than one video tracks, the first one "
                       "will be attachecd to renderer.";
  }

  WebrtcVideoRendererARGBImpl* old_renderer =
      renderer_impl_ ? renderer_impl_ : nullptr;

  renderer_impl_ = new WebrtcVideoRendererARGBImpl(renderer);
  video_tracks[0]->AddOrUpdateSink(renderer_impl_, rtc::VideoSinkWants());

  if (old_renderer)
    delete old_renderer;

  LOG(LS_INFO) << "Attached the stream to a renderer.";
}

#if defined(WEBRTC_WIN)
void Stream::AttachVideoRenderer(VideoRenderWindow& render_window) {
  if (media_stream_ == nullptr) {
    LOG(LS_ERROR) << "Cannot attach an audio only stream to a renderer.";
    return;
  }

  auto video_tracks = media_stream_->GetVideoTracks();
  if (video_tracks.size() == 0) {
    LOG(LS_ERROR) << "Attach failed because of no video tracks.";
    return;
  } else if (video_tracks.size() > 1) {
    LOG(LS_WARNING) << "There are more than one video tracks, the first one "
                       "will be attachecd to renderer.";
  }

  WebrtcVideoRendererD3D9Impl* old_renderer =
      d3d9_renderer_impl_ ? d3d9_renderer_impl_ : nullptr;

  d3d9_renderer_impl_ =
      new WebrtcVideoRendererD3D9Impl(render_window.GetWindowHandle());
  video_tracks[0]->AddOrUpdateSink(d3d9_renderer_impl_, rtc::VideoSinkWants());

  if (old_renderer)
    delete old_renderer;

  LOG(LS_INFO) << "Attached the stream to a renderer.";
}
#endif

void Stream::DetachVideoRenderer() {
#if defined(WEBRTC_WIN)
  if (media_stream_ == nullptr ||
      (renderer_impl_ == nullptr && d3d9_renderer_impl_ == nullptr))
    return;
#else
  if (media_stream_ == nullptr || renderer_impl_ == nullptr)
    return;
#endif

  auto video_tracks = media_stream_->GetVideoTracks();
  if(video_tracks.size() == 0)
    return;

  // Detach from the first stream.
  if (renderer_impl_ != nullptr) {
    video_tracks[0]->RemoveSink(renderer_impl_);
    delete renderer_impl_;
    renderer_impl_ = nullptr;
  }
#if defined(WEBRTC_WIN)
  if (d3d9_renderer_impl_ != nullptr) {
    video_tracks[0]->RemoveSink(d3d9_renderer_impl_);
    delete d3d9_renderer_impl_;
    d3d9_renderer_impl_ = nullptr;
  }
#endif
}

StreamSourceInfo Stream::Source() const {
  return source_;
}

void Stream::AddObserver(StreamObserver& observer) {
  const std::lock_guard<std::mutex> lock(observer_mutex_);
  std::vector<std::reference_wrapper<StreamObserver>>::iterator it =
      std::find_if(
          observers_.begin(), observers_.end(),
          [&](std::reference_wrapper<StreamObserver> o) -> bool {
      return &observer == &(o.get());
  });
  if (it != observers_.end()) {
      LOG(LS_INFO) << "Adding duplicate observer.";
      return;
  }
  observers_.push_back(observer);
}

void Stream::RemoveObserver(StreamObserver& observer) {
  const std::lock_guard<std::mutex> lock(observer_mutex_);
  observers_.erase(std::find_if(
      observers_.begin(), observers_.end(),
      [&](std::reference_wrapper<StreamObserver> o) -> bool {
        return &observer == &(o.get());
      }));
}

void Stream::TriggerOnStreamEnded() {
  ended_ = true;
  for (auto its = observers_.begin(); its != observers_.end(); ++its) {
    (*its).get().OnEnded();
  }
}

LocalStream::LocalStream() : media_constraints_(new MediaConstraintsImpl) {}

LocalStream::LocalStream(MediaStreamInterface* media_stream,
                         StreamSourceInfo source)
    : Stream(media_stream, source) {}

LocalStream::~LocalStream() {
  delete media_constraints_;
}

LocalCameraStream::~LocalCameraStream() {
  LOG(LS_INFO) << "Destroy LocalCameraStream.";
  if (media_stream_ != nullptr) {
    // Remove all tracks before dispose stream.
    auto audio_tracks = media_stream_->GetAudioTracks();
    for (auto it = audio_tracks.begin(); it != audio_tracks.end(); ++it) {
      media_stream_->RemoveTrack(*it);
    }
    auto video_tracks = media_stream_->GetVideoTracks();
    for (auto it = video_tracks.begin(); it != video_tracks.end(); ++it) {
      media_stream_->RemoveTrack(*it);
    }
  }
}

std::shared_ptr<LocalCameraStream> LocalCameraStream::Create(
    const LocalCameraStreamParameters& parameters,
    int& error_code) {
  error_code = 0;
  std::shared_ptr<LocalCameraStream> stream(
      new LocalCameraStream(parameters, error_code));
  if (error_code != 0)
    return nullptr;
  else
    return stream;
}

std::shared_ptr<LocalCameraStream> LocalCameraStream::Create(
    const bool is_audio_enabled,
    webrtc::VideoTrackSourceInterface* video_source,
    int& error_code) {
  error_code = 0;
  std::shared_ptr<LocalCameraStream> stream(
      new LocalCameraStream(is_audio_enabled, video_source, error_code));
  if (error_code != 0)
    return nullptr;
  else
    return stream;
}

LocalCameraStream::LocalCameraStream(
    const LocalCameraStreamParameters& parameters,
    int& error_code) {
  if (!parameters.AudioEnabled() && !parameters.VideoEnabled()) {
    LOG(LS_ERROR)
        << "Cannot create a LocalCameraStream without audio and video.";
    error_code = StreamException::kLocalInvalidOption;
    return;
  }
  scoped_refptr<PeerConnectionDependencyFactory> pcd_factory =
      PeerConnectionDependencyFactory::Get();
  std::string media_stream_id("MediaStream-" + rtc::CreateRandomUuid());
  scoped_refptr<MediaStreamInterface> stream =
      pcd_factory->CreateLocalMediaStream(media_stream_id);
  // Create audio track
  if (parameters.AudioEnabled()) {
    std::string audio_track_id(rtc::CreateRandomUuid());
    scoped_refptr<AudioTrackInterface> audio_track =
        pcd_factory->CreateLocalAudioTrack("AudioTrack-" + audio_track_id);
    stream->AddTrack(audio_track);
  }
  // Create video track.
  if (parameters.VideoEnabled()) {
#if !defined(WEBRTC_IOS)
    cricket::WebRtcVideoDeviceCapturerFactory factory;
    std::unique_ptr<cricket::VideoCapturer> capturer(nullptr);
    if (!parameters.CameraId().empty()) {
      // TODO(jianjun): When create capturer, we will compare ID first. If
      // failed, fallback to compare name. Comparing name is deprecated, remove
      // it when it is old enough.
      capturer = factory.Create(
          cricket::Device(parameters.CameraId(), parameters.CameraId()));
    }
    if (!capturer) {
      LOG(LS_ERROR)
          << "Cannot open video capturer " << parameters.CameraId()
          << ". Please make sure camera ID is correct and it is not in use.";
      error_code = StreamException::kLocalDeviceNotFound;
      return;
    }
    // Check supported resolution
    auto supported_resolution =
        DeviceUtils::VideoCapturerSupportedResolutions(parameters.CameraId());
    auto resolution_iterator = std::find(
        supported_resolution.begin(), supported_resolution.end(),
        Resolution(parameters.ResolutionWidth(), parameters.ResolutionHeight()));
    if (resolution_iterator == supported_resolution.end()) {
      LOG(LS_ERROR) << "Resolution " << parameters.ResolutionWidth() << "x"
                    << parameters.ResolutionHeight()
                    << " is not supported by video capturer "
                    << parameters.CameraId();
      error_code = StreamException::kLocalNotSupported;
      return;
    }
    media_constraints_->SetMandatory(
        webrtc::MediaConstraintsInterface::kMaxWidth,
        std::to_string(parameters.ResolutionWidth()));
    media_constraints_->SetMandatory(
        webrtc::MediaConstraintsInterface::kMaxHeight,
        std::to_string(parameters.ResolutionHeight()));
    media_constraints_->SetMandatory(
        webrtc::MediaConstraintsInterface::kMinWidth,
        std::to_string(parameters.ResolutionWidth()));
    media_constraints_->SetMandatory(
        webrtc::MediaConstraintsInterface::kMinHeight,
        std::to_string(parameters.ResolutionHeight()));

    scoped_refptr<VideoTrackSourceInterface> source =
        pcd_factory->CreateVideoSource(std::move(capturer), media_constraints_);
#else
    capturer_ = ObjcVideoCapturerFactory::Create(parameters);
    if (!capturer_) {
      LOG(LS_ERROR) << "Failed to create capturer. Please check parameters.";
      error_code = StreamException::kLocalNotSupported;
      return;
    }
    scoped_refptr<VideoTrackSourceInterface> source = capturer_->source();
#endif
    std::string video_track_id("VideoTrack-" + rtc::CreateRandomUuid());
    scoped_refptr<VideoTrackInterface> video_track =
        pcd_factory->CreateLocalVideoTrack(video_track_id, source);
    stream->AddTrack(video_track);
  }
  source_.video = VideoSourceInfo::kCamera;
  source_.audio = AudioSourceInfo::kMic;
  media_stream_ = stream;
  media_stream_->AddRef();
}

LocalCameraStream::LocalCameraStream(
      const bool is_audio_enabled,
      webrtc::VideoTrackSourceInterface* video_source,
      int& error_code){
  RTC_CHECK(video_source);
  scoped_refptr<PeerConnectionDependencyFactory> pcd_factory =
      PeerConnectionDependencyFactory::Get();
  std::string media_stream_id("MediaStream-" + rtc::CreateRandomUuid());
  scoped_refptr<MediaStreamInterface> stream =
      pcd_factory->CreateLocalMediaStream(media_stream_id);
  // Create audio track
  if (is_audio_enabled) {
    std::string audio_track_id(rtc::CreateRandomUuid());
    scoped_refptr<AudioTrackInterface> audio_track =
        pcd_factory->CreateLocalAudioTrack("AudioTrack-" + audio_track_id);
    stream->AddTrack(audio_track);
  }
  std::string video_track_id("VideoTrack-" + rtc::CreateRandomUuid());
  scoped_refptr<VideoTrackInterface> video_track =
      pcd_factory->CreateLocalVideoTrack(video_track_id, video_source);
  stream->AddTrack(video_track);
  media_stream_ = stream;
  media_stream_->AddRef();
}

void LocalCameraStream::Close() {
  RTC_CHECK(media_stream_);

  DetachVideoRenderer();

  for (auto const& audio_track : media_stream_->GetAudioTracks())
    media_stream_->RemoveTrack(audio_track);

  for (auto const& video_track : media_stream_->GetVideoTracks())
    media_stream_->RemoveTrack(video_track);
}

LocalScreenStream::~LocalScreenStream() {
  LOG(LS_INFO) << "Destory LocalScreenStream.";
  if (media_stream_ != nullptr) {
    // Remove all tracks before dispose stream.
    auto audio_tracks = media_stream_->GetAudioTracks();
    for (auto it = audio_tracks.begin(); it != audio_tracks.end(); ++it)
      media_stream_->RemoveTrack(*it);

    auto video_tracks = media_stream_->GetVideoTracks();
    for (auto it = video_tracks.begin(); it != video_tracks.end(); ++it)
      media_stream_->RemoveTrack(*it);
  }
}

LocalScreenStream::LocalScreenStream(
    std::shared_ptr<LocalDesktopStreamParameters> parameters,
    std::unique_ptr<LocalScreenStreamObserver> observer) {
  if (!parameters->VideoEnabled() && !parameters->AudioEnabled()) {
    LOG(LS_WARNING) << "Create LocalScreenStream without video and audio.";
  }
  scoped_refptr<PeerConnectionDependencyFactory> factory =
      PeerConnectionDependencyFactory::Get();
  std::string media_stream_id("MediaStream-" + rtc::CreateRandomUuid());
  scoped_refptr<MediaStreamInterface> stream =
      factory->CreateLocalMediaStream(media_stream_id);
  std::unique_ptr<BasicDesktopCapturer> capturer(nullptr);
  if (parameters->VideoEnabled()) {
    webrtc::DesktopCaptureOptions options =
        webrtc::DesktopCaptureOptions::CreateDefault();
    // options.set_allow_directx_capturer(true);
    if (parameters->SourceType() ==
        LocalDesktopStreamParameters::DesktopSourceType::kFullScreen) {
      capturer = std::unique_ptr<BasicScreenCapturer>(new BasicScreenCapturer(options));
    } else {
      capturer = std::unique_ptr<BasicWindowCapturer>(new BasicWindowCapturer(options, std::move(observer)));
    }
    capturer->Init();
    scoped_refptr<VideoTrackSourceInterface> source =
        factory->CreateVideoSource(std::move(capturer), nullptr);
    std::string video_track_id("VideoTrack-" + rtc::CreateRandomUuid());
    scoped_refptr<VideoTrackInterface> video_track =
        factory->CreateLocalVideoTrack(video_track_id, source);
    stream->AddTrack(video_track);
  }

  if (parameters->AudioEnabled()) {
    std::string audio_track_id("AudioTrack-" + rtc::CreateRandomUuid());
    scoped_refptr<AudioTrackInterface> audio_track =
        factory->CreateLocalAudioTrack(audio_track_id);
    stream->AddTrack(audio_track);
  }
  media_stream_ = stream;
  media_stream_->AddRef();
}

LocalCustomizedStream::~LocalCustomizedStream() {
  LOG(LS_INFO) << "Destory LocalCameraStream.";
  if (media_stream_ != nullptr) {
    // Remove all tracks before dispose stream.
    auto audio_tracks = media_stream_->GetAudioTracks();
    for (auto it = audio_tracks.begin(); it != audio_tracks.end(); ++it)
      media_stream_->RemoveTrack(*it);

    auto video_tracks = media_stream_->GetVideoTracks();
    for (auto it = video_tracks.begin(); it != video_tracks.end(); ++it)
      media_stream_->RemoveTrack(*it);
  }
}

LocalCustomizedStream::LocalCustomizedStream(
    std::shared_ptr<LocalCustomizedStreamParameters> parameters,
    std::unique_ptr<VideoFrameGeneratorInterface> framer) {
  if (!parameters->VideoEnabled() && !parameters->AudioEnabled()) {
    LOG(LS_WARNING) << "Create LocalCameraStream without video and audio.";
  }
  scoped_refptr<PeerConnectionDependencyFactory> pcd_factory =
      PeerConnectionDependencyFactory::Get();
  std::string media_stream_id("MediaStream-" + rtc::CreateRandomUuid());
  scoped_refptr<MediaStreamInterface> stream =
      pcd_factory->CreateLocalMediaStream(media_stream_id);
  std::unique_ptr<CustomizedFramesCapturer> capturer(nullptr);
  if (parameters->VideoEnabled()) {
    capturer = std::unique_ptr<CustomizedFramesCapturer>(
        new CustomizedFramesCapturer(std::move(framer)));
    capturer->Init();
    scoped_refptr<VideoTrackSourceInterface> source =
    pcd_factory->CreateVideoSource(std::move(capturer), nullptr);
    std::string video_track_id("VideoTrack-" + rtc::CreateRandomUuid());
    scoped_refptr<VideoTrackInterface> video_track =
        pcd_factory->CreateLocalVideoTrack(video_track_id, source);
    stream->AddTrack(video_track);
  }

  if (parameters->AudioEnabled()) {
    std::string audio_track_id("AudioTrack-" + rtc::CreateRandomUuid());
    scoped_refptr<AudioTrackInterface> audio_track =
        pcd_factory->CreateLocalAudioTrack(audio_track_id);
    stream->AddTrack(audio_track);
  }
  media_stream_ = stream;
  media_stream_->AddRef();
}

LocalCustomizedStream::LocalCustomizedStream(
    std::shared_ptr<LocalCustomizedStreamParameters> parameters,
    VideoEncoderInterface* encoder) {
  if (!parameters->VideoEnabled() && !parameters->AudioEnabled()) {
    LOG(LS_WARNING) << "Create LocalCameraStream without video and audio.";
  }
  scoped_refptr<PeerConnectionDependencyFactory> pcd_factory =
      PeerConnectionDependencyFactory::Get();
  std::string media_stream_id("MediaStream-" + rtc::CreateRandomUuid());
  scoped_refptr<MediaStreamInterface> stream =
      pcd_factory->CreateLocalMediaStream(media_stream_id);
  std::unique_ptr<CustomizedFramesCapturer> capturer(nullptr);
  if (parameters->VideoEnabled()) {
    encoded_ = true;
    capturer = std::unique_ptr<CustomizedFramesCapturer>(new CustomizedFramesCapturer(
      parameters->ResolutionWidth(),
      parameters->ResolutionHeight(),
      parameters->Fps(),
      parameters->Bitrate(),
      encoder));
    capturer->Init();
    scoped_refptr<VideoTrackSourceInterface> source =
        pcd_factory->CreateVideoSource(std::move(capturer), nullptr);
    std::string video_track_id("VideoTrack-" + rtc::CreateRandomUuid());
    scoped_refptr<VideoTrackInterface> video_track =
        pcd_factory->CreateLocalVideoTrack(video_track_id, source);
    stream->AddTrack(video_track);
  }

  if (parameters->AudioEnabled()) {
    std::string audio_track_id("AudioTrack-" + rtc::CreateRandomUuid());
    scoped_refptr<AudioTrackInterface> audio_track =
        pcd_factory->CreateLocalAudioTrack(audio_track_id);
    stream->AddTrack(audio_track);
  }
  media_stream_ = stream;
  media_stream_->AddRef();
}

void LocalCustomizedStream::AttachVideoRenderer(
    VideoRendererARGBInterface& renderer) {
  if (encoded_) {
    LOG(LS_ERROR) << "Not attaching renderer to encoded stream.";
    return;
  }
  if (media_stream_ == nullptr) {
    RTC_DCHECK(false);
    LOG(LS_ERROR) << "Cannot attach an audio only stream to a renderer.";
    return;
  }

  auto video_tracks = media_stream_->GetVideoTracks();
  if (video_tracks.size() == 0) {
    LOG(LS_ERROR) << "Attach failed because of no video tracks.";
    return;
  } else if (video_tracks.size() > 1) {
    LOG(LS_WARNING) << "There are more than one video tracks, the first one "
                       "will be attachecd to renderer.";
  }

  WebrtcVideoRendererARGBImpl* old_renderer =
      renderer_impl_ ? renderer_impl_ : nullptr;

  renderer_impl_ = new WebrtcVideoRendererARGBImpl(renderer);
  video_tracks[0]->AddOrUpdateSink(renderer_impl_, rtc::VideoSinkWants());

  if (old_renderer)
    delete old_renderer;

  LOG(LS_INFO) << "Attached the stream to a renderer.";
}

#if defined(WEBRTC_WIN)
void LocalCustomizedStream::AttachVideoRenderer(
    VideoRenderWindow& render_window) {
  if (encoded_) {
    LOG(LS_ERROR) << "Not attaching renderer to encoded stream.";
    return;
  }
  if (media_stream_ == nullptr) {
    RTC_DCHECK(false);
    LOG(LS_ERROR) << "Cannot attach an audio only stream to a renderer.";
    return;
  }

  auto video_tracks = media_stream_->GetVideoTracks();
  if (video_tracks.size() == 0) {
    LOG(LS_ERROR) << "Attach failed because of no video tracks.";
    return;
  } else if (video_tracks.size() > 1) {
    LOG(LS_WARNING) << "There are more than one video tracks, the first one "
                       "will be attachecd to renderer.";
  }

  WebrtcVideoRendererD3D9Impl* old_renderer =
      d3d9_renderer_impl_ ? d3d9_renderer_impl_ : nullptr;

  d3d9_renderer_impl_ =
      new WebrtcVideoRendererD3D9Impl(render_window.GetWindowHandle());
  video_tracks[0]->AddOrUpdateSink(d3d9_renderer_impl_, rtc::VideoSinkWants());

  if (old_renderer)
    delete old_renderer;

  LOG(LS_INFO) << "Attached the stream to a renderer.";
}
#endif

void LocalCustomizedStream::DetachVideoRenderer() {
  if (encoded_) {
    LOG(LS_ERROR) << "Not attaching renderer to encoded stream.";
    return;
  }
#if defined(WEBRTC_WIN)
  if (media_stream_ == nullptr ||
      (renderer_impl_ == nullptr && d3d9_renderer_impl_ == nullptr))
    return;
#else
  if (media_stream_ == nullptr || renderer_impl_ == nullptr)
    return;
#endif

  auto video_tracks = media_stream_->GetVideoTracks();
  if (video_tracks.size() == 0)
    return;

  // Detach from the first stream.
  if (renderer_impl_ != nullptr) {
    video_tracks[0]->RemoveSink(renderer_impl_);
    delete renderer_impl_;
    renderer_impl_ = nullptr;
  }
#if defined(WEBRTC_WIN)
  if (d3d9_renderer_impl_ != nullptr) {
    video_tracks[0]->RemoveSink(d3d9_renderer_impl_);
    delete d3d9_renderer_impl_;
    d3d9_renderer_impl_ = nullptr;
  }
#endif
}


RemoteStream::RemoteStream(MediaStreamInterface* media_stream,
                           const std::string& from)
    : origin_(from) {
  RTC_CHECK(media_stream);
  Id(media_stream->label());
  media_stream_ = media_stream;
  media_stream_->AddRef();
}

RemoteStream::RemoteStream(const std::string& id, const std::string& from,
                           const ics::base::SubscriptionCapabilities& subscription_capabilities,
                           const ics::base::PublicationSettings& publication_settings)
    : Stream(id),
      origin_(from),
      subscription_capabilities_(subscription_capabilities),
      publication_settings_(publication_settings) {}

std::string RemoteStream::Origin() {
  return origin_;
}

void RemoteStream::MediaStream(
    MediaStreamInterface* media_stream) {
  Stream::MediaStream(media_stream);
}

MediaStreamInterface* RemoteStream::MediaStream() {
  return media_stream_;
}

RemoteCameraStream::RemoteCameraStream(std::string& id, std::string& from,
                              const ics::base::SubscriptionCapabilities& subscription_capabilities,
                              const ics::base::PublicationSettings& publication_settings)
    : RemoteStream(id, from, subscription_capabilities, publication_settings) {}

RemoteCameraStream::RemoteCameraStream(MediaStreamInterface* media_stream,
                                       std::string& from)
    : RemoteStream(media_stream, from) {}

RemoteScreenStream::RemoteScreenStream(std::string& id, std::string& from,
                              const ics::base::SubscriptionCapabilities& subscription_capabilities,
                              const ics::base::PublicationSettings& publication_settings)
    : RemoteStream(id, from, subscription_capabilities, publication_settings) {}

RemoteScreenStream::RemoteScreenStream(MediaStreamInterface* media_stream,
                                       std::string& from)
    : RemoteStream(media_stream, from) {}
}
}