/* net_voip.h */
#ifndef ENET_VOIP_H
#define ENET_VOIP_H

#include "core/error_macros.h"
#include "core/io/networked_multiplayer_peer.h"
#include "core/print_string.h"
#include "core/reference.h"
#include "core/ustring.h"
#include "core/vector.h"
#include "scene/main/timer.h"

#include <opus.h>

#include "audio_stream_enet_voip.h"
#include "enet_voip_enum.h"

class EnetVoip : public Reference {
	GDCLASS(EnetVoip, Reference);

protected:
	static void _bind_methods();

public:
	EnetVoip();
	~EnetVoip();
	void set_network_peer(const Ref<NetworkedMultiplayerPeer> &p_network_peer);
	Vector<int> get_network_connected_peers() const;
	bool is_network_server() const;
	bool has_network_peer() const;
	int get_network_unique_id() const;
	void send_user_info();
	void poll();
	//VOIP
	void talk();
	void mute();
	//Text
	void send_text(String msg);

private:
	int last_send_cache_id;
	Ref<NetworkedMultiplayerPeer> network_peer;
	void _send_user_info(int p_to);
	template <class packetBuilder>
	void _send_packet(int p_to, PacketType type, packetBuilder &message, NetworkedMultiplayerPeer::TransferMode transfer);
	void _network_process_packet(int p_from, const uint8_t *p_packet, int p_packet_len);
	void _network_poll();
	void _network_peer_connected(int p_id);
	void _network_peer_disconnected(int p_id);
	void _connected_to_server();
	void _connection_failed();
	void _server_disconnected();
	//voip
	int outgoing_sequence_number;
	uint64_t last_sent_audio_timestamp;
	//encoders
	enum {
		SAMPLE_RATE = 48000, //44100 crashes in the constructor
		BIT_RATE = 16000, //https://wiki.xiph.org/Opus_Recommended_Settings#Bandwidth_Transition_Thresholds
		FRAME_SIZE = 960 //https://www.opus-codec.org/docs/html_api/group__opusencoder.html#ga88621a963b809ebfc27887f13518c966
	};
	OpusDecoder *opusDecoder;
	OpusEncoder *opusEncoder;
	void reset_encoder();
	void _create_audio_frame(Vector<uint8_t> pcm);
	int _encode_audio_frame(int target, Vector<uint8_t> &pcm);
	void _process_audio_packet(int p_from, const uint8_t *p_packet, int p_packet_len);
	Pair<int, bool> _decode_opus_frame(const uint8_t *in_buf, int in_len, int16_t *pcm_buf, int buf_len);
};

#endif