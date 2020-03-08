/* enet_voip.cpp */
#include "core/io/marshalls.h"
#include "core/os/os.h"
#include "core/pair.h"
#include "servers/audio/audio_stream.h"

#include "enet_voip.h"
#include "flatbuffers/flatbuffers.h"
#include "godotvoip_generated.h"

void EnetVoip::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_network_peer", "peer"), &EnetVoip::set_network_peer);
	ClassDB::bind_method(D_METHOD("is_network_server"), &EnetVoip::is_network_server);
	ClassDB::bind_method(D_METHOD("has_network_peer"), &EnetVoip::has_network_peer);
	ClassDB::bind_method(D_METHOD("get_network_connected_peers"), &EnetVoip::get_network_connected_peers);
	ClassDB::bind_method(D_METHOD("get_network_unique_id"), &EnetVoip::get_network_unique_id);
	ClassDB::bind_method(D_METHOD("send_user_info"), &EnetVoip::send_user_info);
	ClassDB::bind_method(D_METHOD("poll"), &EnetVoip::poll);

	ClassDB::bind_method(D_METHOD("_network_peer_connected"), &EnetVoip::_network_peer_connected);
	ClassDB::bind_method(D_METHOD("_network_peer_disconnected"), &EnetVoip::_network_peer_disconnected);
	ClassDB::bind_method(D_METHOD("_connected_to_server"), &EnetVoip::_connected_to_server);
	ClassDB::bind_method(D_METHOD("_connection_failed"), &EnetVoip::_connection_failed);
	ClassDB::bind_method(D_METHOD("_server_disconnected"), &EnetVoip::_server_disconnected);

	ADD_SIGNAL(MethodInfo("connected_to_server"));
	ADD_SIGNAL(MethodInfo("connection_failed"));
	ADD_SIGNAL(MethodInfo("server_disconncted"));
	ADD_SIGNAL(MethodInfo("network_peer_connected", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("network_peer_disconnected", PropertyInfo(Variant::INT, "id")));
	//Text
	ClassDB::bind_method(D_METHOD("send_text", "message"), &EnetVoip::send_text);
	ADD_SIGNAL(MethodInfo("text_message", PropertyInfo(Variant::STRING, "message"), PropertyInfo(Variant::INT, "sender_id")));
	//VOIP

	ClassDB::bind_method(D_METHOD("talk"), &EnetVoip::talk);
	ClassDB::bind_method(D_METHOD("mute"), &EnetVoip::mute);
}

EnetVoip::EnetVoip() :
		last_sent_audio_timestamp(0) {
	int error;
	opusDecoder = opus_decoder_create(EnetVoip::SAMPLE_RATE, 1, &error);
	if (error != OPUS_OK) {
		ERR_PRINT("failed to initialize OPUS decoder: " + String(opus_strerror(error)));
	}
	opusEncoder = opus_encoder_create(EnetVoip::SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &error);
	if (error != OPUS_OK) {
		ERR_PRINT("failed to initialize OPUS encoder: " + String(opus_strerror(error)));
	}
	opus_encoder_ctl(opusEncoder, OPUS_SET_VBR(0));
	error = opus_encoder_ctl(opusEncoder, OPUS_SET_BITRATE(EnetVoip::BIT_RATE));
	if (error != OPUS_OK) {
		ERR_PRINT("failed to initialize bitrate to " + itos(EnetVoip::BIT_RATE) + "B/s: " + String(opus_strerror(error)));
	}
	reset_encoder();
}
EnetVoip::~EnetVoip() {
	if (opusDecoder) {
		opus_decoder_destroy(opusDecoder);
	}

	if (opusEncoder) {
		opus_encoder_destroy(opusEncoder);
	}
}
void EnetVoip::reset_encoder() {
	int status = opus_encoder_ctl(opusEncoder, OPUS_RESET_STATE, nullptr);

	if (status != OPUS_OK) {
		ERR_PRINT("failed to reset encoder: " + String(opus_strerror(status)));
	}
	outgoing_sequence_number = 0;
}
void EnetVoip::send_text(String msg) {
	CharString m = msg.utf8();
	flatbuffers::FlatBufferBuilder builder;

	auto message = builder.CreateSharedString(m.get_data(), m.length());
	auto textMessage = GodotProto::CreateTextMessage(builder, get_network_unique_id(), 0, 0, message);
	auto rootPacket = GodotProto::CreatePacket(builder, GodotProto::PacketPayload_TextMessage, textMessage.Union());
	builder.Finish(rootPacket);

	_send_packet<flatbuffers::FlatBufferBuilder>(0, PacketType::TEXTMESSAGE, builder, NetworkedMultiplayerPeer::TRANSFER_MODE_RELIABLE);
}
void EnetVoip::send_user_info() {
	_send_user_info(0);
}
void EnetVoip::_send_user_info(int p_to) {
}

template <class packetBuilder>
void EnetVoip::_send_packet(int p_to, PacketType type, packetBuilder &message, NetworkedMultiplayerPeer::TransferMode transferMode) {
	network_peer->set_transfer_mode(transferMode);
	network_peer->set_target_peer(p_to);
	network_peer->put_packet(message.GetBufferPointer(), message.GetSize());
}

void EnetVoip::_network_process_packet(int p_from, const uint8_t *p_packet, int p_packet_len) {
	flatbuffers::Verifier packetvert(p_packet, p_packet_len);
	bool ok = GodotProto::VerifyPacketBuffer(packetvert);
	auto packet = GodotProto::GetPacket(p_packet);
	if(ok) {
		switch (packet->payload_type()) {
			case GodotProto::PacketPayload_AudioPacket: {
				//_process_audio_packet(p_from, proto_packet, proto_packet_len);
			} break;
			case GodotProto::PacketPayload_TextMessage: {
				auto txt = packet->payload_as_TextMessage();
				auto s = txt -> message();
				String msg = String::utf8(s->c_str(), s->Length());
				this->emit_signal("text_message", msg, p_from);
			} break;
			case GodotProto::PacketPayload_Version: {

			} break;
			case GodotProto::PacketPayload_UserInfo: {
			}
			default:
				break;
		}
	} else {
		WARN_PRINT("Malformed VOIP Packet: " +itos(p_from));
	}
}

bool EnetVoip::is_network_server() const {

	ERR_FAIL_COND_V(!network_peer.is_valid(), false);
	return network_peer->is_server();
}

bool EnetVoip::has_network_peer() const {
	return network_peer.is_valid();
}

void EnetVoip::set_network_peer(const Ref<NetworkedMultiplayerPeer> &p_network_peer) {
	if (network_peer.is_valid()) {
		network_peer->disconnect("peer_connected", callable_mp(this, &EnetVoip::_network_peer_connected));
		network_peer->disconnect("peer_disconnected", callable_mp(this, &EnetVoip::_network_peer_disconnected));
		network_peer->disconnect("connection_succeeded", callable_mp(this, &EnetVoip::_connected_to_server));
		network_peer->disconnect("connection_failed", callable_mp(this, &EnetVoip::_connection_failed));
		network_peer->disconnect("server_disconnected", callable_mp(this, &EnetVoip::_server_disconnected));
		last_send_cache_id = 1;
	}

	WARN_PRINT("Supplied NetworkedNetworkPeer must be connecting or connected.");
	ERR_FAIL_COND(p_network_peer.is_valid() && p_network_peer->get_connection_status() == NetworkedMultiplayerPeer::CONNECTION_DISCONNECTED);
	network_peer = p_network_peer;
	if (network_peer.is_valid()) {
		network_peer->connect("peer_connected", callable_mp(this, &EnetVoip::_network_peer_connected));
		network_peer->connect("peer_disconnected", callable_mp(this, &EnetVoip::_network_peer_disconnected));
		network_peer->connect("connection_succeeded", callable_mp(this, &EnetVoip::_connected_to_server));
		network_peer->connect("connection_failed", callable_mp(this, &EnetVoip::_connection_failed));
		network_peer->connect("server_disconnected", callable_mp(this, &EnetVoip::_server_disconnected));
	}
}
void EnetVoip::_connection_failed() {
	emit_signal("connection_failed");
}
void EnetVoip::_connected_to_server() {
	emit_signal("connected_to_server");
}
void EnetVoip::_network_peer_connected(int p_id) {
	emit_signal("network_peer_connected", p_id);
}
void EnetVoip::_network_peer_disconnected(int p_id) {
	emit_signal("network_peer_disconnected", p_id);
}
void EnetVoip::_server_disconnected() {
	emit_signal("server_disconnected");
}
int EnetVoip::get_network_unique_id() const {
	ERR_FAIL_COND_V(!network_peer.is_valid(), 0);
	return network_peer->get_unique_id();
}
Vector<int> EnetVoip::get_network_connected_peers() const {
	ERR_FAIL_COND_V(!network_peer.is_valid(), Vector<int>());
	Vector<int> ret;
	const int *k = NULL;
	return ret;
}

void EnetVoip::_network_poll() {

	if (!network_peer.is_valid() || network_peer->get_connection_status() == NetworkedMultiplayerPeer::CONNECTION_DISCONNECTED)
		return;

	network_peer->poll();

	if (!network_peer.is_valid()) //it's possible that polling might have resulted in a disconnection, so check here
		return;

	while (network_peer->get_available_packet_count()) {

		int sender = network_peer->get_packet_peer();
		const uint8_t *packet;
		int len;

		Error err = network_peer->get_packet(&packet, len);
		if (err != OK) {
			ERR_PRINT("Error getting packet!");
		}

		_network_process_packet(sender, packet, len);

		if (!network_peer.is_valid()) {
			break; //it's also possible that a packet or RPC caused a disconnection, so also check here
		}
	}
}

void EnetVoip::poll() {
	while (true) {
		//_timer->set_wait_time(500.0/1000.0);
		_network_poll();
		//_timer->start();
	}
}
void EnetVoip::mute() {
	AudioDriver::get_singleton()->capture_stop();
}

void EnetVoip::talk() {
	AudioDriver::get_singleton()->capture_start();
}

Pair<int, bool> EnetVoip::_decode_opus_frame(const uint8_t *in_buf, int in_len, int16_t *pcm_buf, int buf_len) {
	//VarInt varint(in_buf);
	//int16_t opus_length = varint.getValue();
	//int pointer = varint.getEncoded().size();
	//bool endFrame = opus_length != 0x2000;
	//opus_length = opus_length & 0x1fff;
	//int decoded_samples = opus_decode(opusDecoder, (const unsigned char *) &in_buf[pointer], opus_length, pcm_buf, buf_len, 0);
	//return Pair<int,bool>(decoded_samples, endFrame);
	return Pair<int, bool>(0, false);
}

void EnetVoip::_process_audio_packet(int p_from, const uint8_t *p_packet, int p_packet_len) {
	//print_line("from id: "+ itos(p_from));
}

void EnetVoip::_create_audio_frame(Vector<uint8_t> pcm) {
	_encode_audio_frame(0, pcm);
}
int EnetVoip::_encode_audio_frame(int target, Vector<uint8_t> &pcm) {
	int now = OS::get_singleton()->get_ticks_msec();
	if ((now - last_sent_audio_timestamp) > 5000) {
		reset_encoder();
	}
	int16_t pcm_buf[EnetVoip::FRAME_SIZE];
	copymem(pcm_buf, pcm.ptrw(), pcm.size());
	//https://www.opus-codec.org/docs/html_api/group__opusencoder.html#ga88621a963b809ebfc27887f13518c966
	//in_len most be multiples of 120
	//uint8_t opus_buf[1024];
	//const int output_size = opus_encode(opusEncoder, pcm_buf, EnetVoip::FRAME_SIZE, opus_buf, 1024);
	//get self network id

	return 0;
}