#include "core/error_macros.h"
#include "core/print_string.h"

#include "audio_stream_enet_voip.h"


AudioStreamPlaybackEnetVoip::AudioStreamPlaybackEnetVoip() :
		active(false), offset(0) {
}
AudioStreamPlaybackEnetVoip::~AudioStreamPlaybackEnetVoip() {
}

void AudioStreamPlaybackEnetVoip::start(float p_from_pos) {
	active = true;
	seek(p_from_pos);
	_begin_resample();
}
void AudioStreamPlaybackEnetVoip::stop() {
	active = false;
	//base->clear();
}
bool AudioStreamPlaybackEnetVoip::is_playing() const {
	return active;
}
int AudioStreamPlaybackEnetVoip::get_loop_count() const {
	//times it looped
	return 0;
}
float AudioStreamPlaybackEnetVoip::get_playback_position() const {
	return float(offset) / base->mix_rate;
}
void AudioStreamPlaybackEnetVoip::seek(float p_time) {
	//not seeking a stream
}
float AudioStreamPlaybackEnetVoip::get_length() const {
	//if supported, otherwise return 0

	int len = base->get_available_bytes();
	switch (base->format) {
		case AudioStreamEnetVoip::FORMAT_8_BITS: len /= 1; break;
		case AudioStreamEnetVoip::FORMAT_16_BITS: len /= 2; break;
	}
	if (base->stereo) {
		len /= 2;
	}
	return float(len) / float(base->mix_rate);
}

float AudioStreamPlaybackEnetVoip::get_stream_sampling_rate() {
	return base->mix_rate;
}
void AudioStreamPlaybackEnetVoip::_mix_internal(AudioFrame *p_buffer, int p_frames) {
	//print_line("buffer_size: " + itos(base->get_available_bytes()));

	ERR_FAIL_COND(!active);
	if (!active) {
		for (int i = 0; i < p_frames; i++) {
			p_buffer[i] = AudioFrame(0, 0);
		}
		return;
	}
	int len_bytes = base->get_available_bytes();
	int len_frames = len_bytes;
	int max_frame_bytes = p_frames;
	switch (base->format) {
		case AudioStreamEnetVoip::FORMAT_8_BITS: len_frames /= 1; break;
		case AudioStreamEnetVoip::FORMAT_16_BITS:
			len_frames /= 2;
			max_frame_bytes *= 2;
			break;
	}
	if (base->stereo) {
		len_frames /= 2;
		max_frame_bytes *= 2;
	}
	int16_t buf[1024];

	int smaller_buf = MIN(len_frames, p_frames);
	int smaller_bytes = MIN(max_frame_bytes, len_bytes);

	base->get_byte_array((uint8_t *)buf, smaller_bytes);

	for (int i = 0; i < smaller_buf; i++) {
		if (base->stereo) {
			p_buffer[i] = AudioFrame(float(buf[2 * i]) / 32768.0f, float(buf[2 * i + 1]) / 32768.0f);
		} else {
			float sample = float(buf[i]) / 32768.0f;
			p_buffer[i] = AudioFrame(sample, sample);
			//print_line("0: " + rtos(sample) );
		}
	}
	offset += smaller_buf;
	int todo = p_frames - smaller_buf;
	for (int i = 0; i < todo; i++) {
		p_buffer[smaller_buf + i] = AudioFrame(0, 0);
		//trying to figure out false is needed
		active = false;
	}
}
void AudioStreamEnetVoip::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_pid", "pid"), &AudioStreamEnetVoip::set_pid);

	ClassDB::bind_method(D_METHOD("set_format", "format"), &AudioStreamEnetVoip::set_format);
	ClassDB::bind_method(D_METHOD("get_format"), &AudioStreamEnetVoip::get_format);
	ClassDB::bind_method(D_METHOD("clear"), &AudioStreamEnetVoip::clear);
	ClassDB::bind_method(D_METHOD("get_stream_name"), &AudioStreamEnetVoip::get_stream_name);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "format", PROPERTY_HINT_ENUM, "8-Bit,16-Bit"), "set_format", "get_format");
	ADD_SIGNAL(MethodInfo("audio_recieved"));
	BIND_ENUM_CONSTANT(FORMAT_8_BITS);
	BIND_ENUM_CONSTANT(FORMAT_16_BITS);
}
AudioStreamEnetVoip::AudioStreamEnetVoip() :
		format(FORMAT_8_BITS), mix_rate(44100.0), stereo(false) {
}
AudioStreamEnetVoip::~AudioStreamEnetVoip() {
}

Ref<AudioStreamPlayback> AudioStreamEnetVoip::instance_playback() {
	Ref<AudioStreamPlaybackEnetVoip> enet_voip_playback;
	enet_voip_playback.instance();
	enet_voip_playback->base = Ref<AudioStreamEnetVoip>(this);
	return enet_voip_playback;
}

Error AudioStreamEnetVoip::append_data(const uint8_t *pcm_data, int p_bytes) {
	bool entire_array = false;
	if (mutex.try_lock() == OK) {
		entire_array = data_buffer.put_byte_array(pcm_data, p_bytes);
		emit_signal("audio_recieved");
	} else {
		ERR_PRINT("Only one writer should exist.");
		return ERR_BUG;
	}
	if (entire_array) {
		return OK;
	} else {
		WARN_PRINT("Out of Memory: Increase AudioStreamEnetVoip buffer by power of 2.");
		return ERR_OUT_OF_MEMORY;
	}
}
int AudioStreamEnetVoip::get_available_bytes() const {
	//return data->size();
	return (int)data_buffer.size();
}
int AudioStreamEnetVoip::get_byte_array(uint8_t *buf, int size) {
	return data_buffer.get_byte_array(buf, size);
}

String AudioStreamEnetVoip::get_stream_name() const {
	return "EnetVoip: " + itos(id);
}

void AudioStreamEnetVoip::set_format(Format p_format) {
	format = p_format;
}
void AudioStreamEnetVoip::set_mix_rate(float rate) {
	mix_rate = rate;
}
void AudioStreamEnetVoip::set_pid(int p_id) {
	id = p_id;
}
AudioStreamEnetVoip::Format AudioStreamEnetVoip::get_format() const {
	return format;
}
void AudioStreamEnetVoip::clear() {
	data_buffer.clear();
}